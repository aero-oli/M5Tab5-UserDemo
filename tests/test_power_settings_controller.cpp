#include <apps/app_settings_power/power_settings_controller.h>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>

hal::HalBase* hal::Get() { return nullptr; }
using namespace app_settings_power;
using namespace std::chrono_literals;

struct FakePower {
    std::mutex mutex;
    std::condition_variable changed;
    bool holdSample = false, sampling = false, asleep = false, fail = false, failWake = false;
    bool finite = true;
    int samples = 0;
    std::vector<std::pair<PowerAction, uint16_t>> calls;
    std::thread::id actionThread;
    hal::HalBase::PMData_t sample()
    {
        std::unique_lock<std::mutex> lock(mutex);
        sampling = true;
        changed.notify_all();
        changed.wait(lock, [&] { return !holdSample; });
        ++samples;
        hal::HalBase::PMData_t data;
        data.valid = data.hasSample = finite;
        data.busVoltage = finite ? 8.12f : std::numeric_limits<float>::quiet_NaN();
        data.busPower = 2.5f;
        return data;
    }
    hal::PowerResult perform(PowerAction action, uint16_t minutes)
    {
        std::lock_guard<std::mutex> lock(mutex);
        actionThread = std::this_thread::get_id();
        calls.push_back({action, minutes});
        changed.notify_all();
        if (fail) return {false, false, true, "Injected failure"};
        asleep = action == PowerAction::SleepTouch;
        return {true, asleep, true, asleep ? "Display off" : "Simulated shutdown"};
    }
    bool sleeping() { std::lock_guard<std::mutex> lock(mutex); return asleep; }
    bool wake()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (failWake) return false;
        asleep = false;
        return true;
    }
};

static std::unique_ptr<PowerSettingsController> controller(std::shared_ptr<FakePower> fake)
{
    return std::make_unique<PowerSettingsController>(
        [fake] { return fake->sample(); },
        [fake](PowerAction action, uint16_t minutes) { return fake->perform(action, minutes); },
        [fake] { return fake->sleeping(); }, [fake] { return fake->wake(); });
}

template<class Predicate> static void wait(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!predicate()) {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(1ms);
    }
}

static void testConfirmationAndValidation()
{
    auto fake = std::make_shared<FakePower>();
    auto c = controller(fake);
    assert(!c->confirm());
    assert(!c->request(static_cast<PowerAction>(99)));
    assert(!c->request(PowerAction::SleepRtc, 0));
    assert(!c->request(PowerAction::SleepRtc, 61));
    for (auto action : {PowerAction::SleepTouch, PowerAction::SleepShake, PowerAction::SleepRtc, PowerAction::PowerOff}) {
        assert(c->request(action));
        assert(c->state().state == PowerState::Confirming);
        assert(!c->request(PowerAction::PowerOff));
        assert(fake->calls.empty());
        c->cancel();
        assert(!c->confirm());
        assert(c->state().state == PowerState::Ready);
    }
    assert(c->request(PowerAction::SleepRtc, 60));
    assert(c->confirm() && !c->confirm());
    wait([&] { return c->state().state == PowerState::Complete; });
    std::lock_guard<std::mutex> lock(fake->mutex);
    assert(fake->calls.size() == 1 && fake->calls.front().second == 60);
    assert(fake->actionThread != std::this_thread::get_id());
}

static void testBlockedSamplingDoesNotBlockUiOrLoseConfirmedCommand()
{
    auto fake = std::make_shared<FakePower>();
    fake->holdSample = true;
    auto c = controller(fake);
    c->update(0);
    {
        std::unique_lock<std::mutex> lock(fake->mutex);
        assert(fake->changed.wait_for(lock, 2s, [&] { return fake->sampling; }));
    }
    const auto start = std::chrono::steady_clock::now();
    assert(c->request(PowerAction::PowerOff) && c->confirm());
    assert(c->state().state == PowerState::Preparing);
    assert(std::chrono::steady_clock::now() - start < 100ms);
    c.reset();
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        fake->holdSample = false;
        fake->changed.notify_all();
    }
    wait([&] { std::lock_guard<std::mutex> lock(fake->mutex); return fake->calls.size() == 1; });
}

static void testSamplingAndDisplayWake()
{
    auto fake = std::make_shared<FakePower>();
    auto c = controller(fake);
    c->update(0);
    wait([&] { return c->state().hasSample; });
    assert(c->state().sample.busVoltage == 8.12f);
    assert(c->request(PowerAction::SleepTouch) && c->confirm());
    wait([&] { return c->state().state == PowerState::DisplayAsleep; });
    assert(!c->request(PowerAction::PowerOff));
    assert(c->wakeDisplay());
    wait([&] { return c->state().state == PowerState::Ready; });
    assert(!fake->sleeping());
    assert(c->request(PowerAction::SleepTouch) && c->confirm());
    wait([&] { return c->state().state == PowerState::DisplayAsleep; });
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        fake->asleep = false;
        fake->finite = false;
    }
    c->update(500);
    wait([&] { return c->state().state == PowerState::Ready; });
    assert(c->state().hasSample && !c->state().sampleValid);
    assert(c->state().sample.busVoltage == 8.12f);
    assert(PowerSettingsController::formatBatteryInput({7.987f, 1.234f}) == "7.99 V / 1.23 W");
}

static void testFailureAndRecovery()
{
    auto fake = std::make_shared<FakePower>();
    fake->fail = true;
    auto c = controller(fake);
    assert(c->request(PowerAction::SleepShake) && c->confirm());
    wait([&] { return c->state().state == PowerState::Failed; });
    assert(c->state().message == "Injected failure");
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        fake->fail = false;
    }
    assert(c->request(PowerAction::SleepTouch) && c->confirm());
    wait([&] { return c->state().state == PowerState::DisplayAsleep; });
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        fake->failWake = true;
    }
    assert(c->wakeDisplay());
    wait([&] { return c->state().state == PowerState::Failed; });
    assert(c->state().message == "Display wake failed");
    assert(c->state().displayAsleep && !c->request(PowerAction::PowerOff));
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        fake->failWake = false;
    }
    assert(c->wakeDisplay());
    wait([&] { return c->state().state == PowerState::Ready; });
    assert(!c->state().displayAsleep);
}

int main()
{
    testConfirmationAndValidation();
    testBlockedSamplingDoesNotBlockUiOrLoseConfirmedCommand();
    testSamplingAndDisplayWake();
    testFailureAndRecovery();
}
