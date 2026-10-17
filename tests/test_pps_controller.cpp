#include <apps/app_pps/pps_controller.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

// Injected devices keep these tests independent of a real HAL instance.
hal::HalBase* hal::Get() { return nullptr; }

using app_pps::OutputState;
using app_pps::PpsController;
using app_pps::PpsState;
using namespace std::chrono_literals;

class Device : public modules::I2cDevice {
public:
    struct Transaction { bool write; uint8_t reg; };
    Device()
    {
        registers[pps::REG_MODULE_ID_L] = {0x41, 0x10};
        registers[pps::REG_UID_W0] = std::vector<uint8_t>(12, 1);
        registers[pps::REG_MODULE_ENABLE] = {1};
        registers[pps::REG_RUNNING_MODE] = {1};
        registers[pps::REG_DATA_FLAG] = {5};
        setFloat(pps::REG_VOUT_READBACK, 5.01f);
        setFloat(pps::REG_IOUT_READBACK, 0.4f);
        setFloat(pps::REG_TEMP_READBACK, 32.0f);
        setFloat(pps::REG_VIN_READBACK, 24.0f);
        setFloat(pps::REG_VOUT_SET, 5.0f);
        setFloat(pps::REG_IOUT_SET, 1.0f);
    }
    bool probe() override { std::lock_guard<std::mutex> lock(mutex); return present; }
    uint8_t address() const override { return 0x35; }
    bool readRegister(uint8_t reg, uint8_t* data, size_t length) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        waitIfBlocked(lock, reg, false);
        transactions.push_back({false, reg});
        if (!present || failRead == reg || registers[reg].size() < length) {
            return false;
        }
        std::memcpy(data, registers[reg].data(), length);
        return true;
    }
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t length) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        waitIfBlocked(lock, reg, true);
        transactions.push_back({true, reg});
        if (!present || failWrite == reg) {
            return false;
        }
        if (ignoreWrite != reg) {
            registers[reg] = std::vector<uint8_t>(data, data + length);
        }
        if (reg == pps::REG_MODULE_ENABLE && data[0] == 0) {
            ++offWrites;
        }
        if (reg == pps::REG_MODULE_ENABLE && data[0] == 1) {
            ++enableWrites;
        }
        changed.notify_all();
        return true;
    }
    void setFloat(uint8_t reg, float value)
    {
        std::vector<uint8_t> bytes(4);
        std::memcpy(bytes.data(), &value, 4);
        registers[reg] = bytes;
    }
    void block(uint8_t reg, bool write)
    {
        std::lock_guard<std::mutex> lock(mutex);
        blockReg = reg;
        blockWrite = write;
        blocked = false;
        released = false;
    }
    void awaitBlocked()
    {
        std::unique_lock<std::mutex> lock(mutex);
        assert(changed.wait_for(lock, 2s, [&] { return blocked; }));
    }
    void release()
    {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        changed.notify_all();
    }
    void awaitOff(int count)
    {
        std::unique_lock<std::mutex> lock(mutex);
        assert(changed.wait_for(lock, 2s, [&] { return offWrites >= count; }));
    }
    float value(uint8_t reg)
    {
        std::lock_guard<std::mutex> lock(mutex);
        float result;
        std::memcpy(&result, registers[reg].data(), 4);
        return result;
    }
    bool enabled()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return registers[pps::REG_MODULE_ENABLE][0] != 0;
    }
    std::mutex mutex;
    std::map<uint8_t, std::vector<uint8_t>> registers;
    std::vector<Transaction> transactions;
    bool present = true;
    int failRead = -1;
    int failWrite = -1;
    int ignoreWrite = -1;
    int offWrites = 0;
    int enableWrites = 0;

private:
    std::condition_variable changed;
    int blockReg = -1;
    bool blockWrite = false;
    bool blocked = false;
    bool released = false;
    void waitIfBlocked(std::unique_lock<std::mutex>& lock, uint8_t reg, bool write)
    {
        if (blockReg != reg || blockWrite != write) {
            return;
        }
        blockReg = -1;
        blocked = true;
        changed.notify_all();
        assert(changed.wait_for(lock, 2s, [&] { return released; }));
    }
};

template<typename Predicate> void eventually(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!predicate()) {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(1ms);
    }
}

struct Fixture {
    std::shared_ptr<Device> device = std::make_shared<Device>();
    // Shared clock outlives the controller if Back leaves an I2C transaction in flight.
    std::shared_ptr<std::atomic<uint32_t>> now = std::make_shared<std::atomic<uint32_t>>(200);
    std::unique_ptr<PpsController> controller;
    Fixture() : controller(std::make_unique<PpsController>(device, [clock = now] { return clock->load(); })) {}
    void ready()
    {
        controller->update(now->load());
        eventually([&] { return PpsController::canEnable(controller->state()); });
    }
    void poll()
    {
        now->fetch_add(200);
        controller->update(now->load());
    }
    void commandDone()
    {
        eventually([&] { return !controller->state().commandPending; });
    }
};

static void initial_connection_disables_output_and_rejects_wrong_identity()
{
    Fixture fixture;
    assert(fixture.device->enabled());
    assert(fixture.controller->state().output == OutputState::Unknown);
    fixture.ready();
    assert(!fixture.device->enabled());
    assert(fixture.controller->state().output == OutputState::Disabled);
    assert(fixture.controller->state().identityValid);

    Fixture wrong;
    {
        std::lock_guard<std::mutex> lock(wrong.device->mutex);
        wrong.device->registers[pps::REG_MODULE_ID_L] = {0x01, 0x00};
    }
    wrong.poll();
    eventually([&] { return wrong.controller->state().telemetry.lastError == "Unexpected module ID at I2C 0x35"; });
    assert(wrong.device->enabled());
    assert(!PpsController::canEnable(wrong.controller->state()));
    wrong.controller.reset();
    eventually([&] { return wrong.device.use_count() == 1; });
    assert(wrong.device->enabled());
    {
        std::lock_guard<std::mutex> lock(wrong.device->mutex);
        for (const auto& transaction : wrong.device->transactions) {
            assert(!transaction.write);
        }
    }
}

static void failure_preserves_last_good_but_marks_output_unknown_and_reconnects_safely()
{
    Fixture fixture;
    fixture.ready();
    assert(fixture.controller->setOutputEnabled(true, 200));
    fixture.commandDone();
    assert(fixture.device->enabled());
    assert(!fixture.controller->state().telemetryFresh);
    {
        std::lock_guard<std::mutex> lock(fixture.device->mutex);
        fixture.device->present = false;
    }
    fixture.poll();
    eventually([&] { return !fixture.controller->state().telemetry.connected; });
    auto state = fixture.controller->state();
    assert(state.output == OutputState::Unknown);
    assert(state.telemetry.outputEnabled); // last known, not a fresh output claim
    assert(state.telemetry.voutReadback == 5.01f);
    assert(!fixture.controller->applySetpoints(400));
    assert(!fixture.controller->setOutputEnabled(true, 400));
    assert(fixture.controller->emergencyOff(400));
    fixture.commandDone();
    assert(fixture.controller->state().lastCommandError.find("may still be live") != std::string::npos);
    {
        std::lock_guard<std::mutex> lock(fixture.device->mutex);
        fixture.device->present = true;
    }
    fixture.poll();
    eventually([&] { return PpsController::canEnable(fixture.controller->state()); });
    assert(!fixture.device->enabled());
    fixture.now->fetch_add(1001);
    assert(fixture.controller->state().output == OutputState::Unknown);
    assert(!fixture.controller->setOutputEnabled(true, 1601));
}

static void partial_and_missed_writes_cannot_enable_output()
{
    Fixture fixture;
    fixture.ready();
    fixture.controller->applyPreset(12.0f, 2.0f);
    assert(!fixture.controller->setOutputEnabled(true, 200));
    {
        std::lock_guard<std::mutex> lock(fixture.device->mutex);
        fixture.device->failWrite = pps::REG_IOUT_SET;
    }
    assert(fixture.controller->applySetpoints(200));
    fixture.commandDone();
    assert(fixture.device->value(pps::REG_VOUT_SET) == 12.0f);
    assert(fixture.device->value(pps::REG_IOUT_SET) == 1.0f);
    assert(!fixture.device->enabled());
    assert(!fixture.controller->state().setpointsVerified);
    fixture.poll();
    eventually([&] { return fixture.controller->state().lastGoodPollMs == fixture.now->load(); });
    assert(!fixture.controller->setOutputEnabled(true, 400));
    {
        std::lock_guard<std::mutex> lock(fixture.device->mutex);
        fixture.device->failWrite = -1;
        fixture.device->ignoreWrite = pps::REG_IOUT_SET;
    }
    assert(fixture.controller->applySetpoints(400));
    fixture.commandDone();
    assert(!fixture.controller->state().setpointsVerified);
    {
        std::lock_guard<std::mutex> lock(fixture.device->mutex);
        fixture.device->ignoreWrite = -1;
    }
    assert(fixture.controller->applySetpoints(400));
    fixture.commandDone();
    assert(PpsController::canEnable(fixture.controller->state()));
    assert(fixture.device->value(pps::REG_IOUT_SET) == 2.0f);
}

static void concurrent_edits_survive_apply_completion()
{
    Fixture fixture;
    fixture.ready();
    fixture.controller->applyPreset(9.0f, 2.0f);
    fixture.device->block(pps::REG_VOUT_SET, true);
    assert(fixture.controller->applySetpoints(200));
    fixture.device->awaitBlocked();
    fixture.controller->setPendingVoltage(12.0f);
    assert(fixture.controller->state().pendingVoltage == 12.0f);
    fixture.device->release();
    fixture.commandDone();
    assert(fixture.device->value(pps::REG_VOUT_SET) == 9.0f);
    assert(fixture.controller->state().setpointsDirty);
    assert(fixture.controller->state().pendingVoltage == 12.0f);
    assert(!fixture.controller->setOutputEnabled(true, 200));
    fixture.controller->setPendingVoltage(std::numeric_limits<float>::quiet_NaN());
    assert(fixture.controller->state().pendingVoltage == 12.0f);
    fixture.controller->setPendingCurrent(12.0f);
    assert(fixture.controller->state().pendingCurrent == 5.0f);
}

static void emergency_off_interrupts_poll_without_waiting_on_caller()
{
    Fixture fixture;
    fixture.ready();
    assert(fixture.controller->setOutputEnabled(true, 200));
    fixture.commandDone();
    fixture.device->block(pps::REG_VOUT_READBACK, false);
    fixture.poll();
    fixture.device->awaitBlocked();
    const auto start = std::chrono::steady_clock::now();
    assert(fixture.controller->emergencyOff(400));
    assert(std::chrono::steady_clock::now() - start < 100ms);
    fixture.device->release();
    fixture.commandDone();
    assert(!fixture.device->enabled());
    assert(fixture.controller->state().output == OutputState::Disabled);
    std::lock_guard<std::mutex> lock(fixture.device->mutex);
    const auto& transactions = fixture.device->transactions;
    size_t interruptedRead = transactions.size();
    for (size_t i = 0; i < transactions.size(); ++i) {
        if (!transactions[i].write && transactions[i].reg == pps::REG_VOUT_READBACK) {
            interruptedRead = i;
        }
    }
    assert(interruptedRead + 1 < transactions.size());
    assert(transactions[interruptedRead + 1].write);
    assert(transactions[interruptedRead + 1].reg == pps::REG_MODULE_ENABLE);
}

static void close_cancels_queued_enable_and_keeps_worker_resources_alive()
{
    Fixture fixture;
    fixture.ready();
    fixture.device->block(pps::REG_VOUT_READBACK, false);
    fixture.poll();
    fixture.device->awaitBlocked();
    assert(fixture.controller->setOutputEnabled(true, 400));
    const auto start = std::chrono::steady_clock::now();
    fixture.controller.reset();
    assert(std::chrono::steady_clock::now() - start < 100ms);
    fixture.device->release();
    fixture.device->awaitOff(2);
    assert(!fixture.device->enabled());
    std::lock_guard<std::mutex> lock(fixture.device->mutex);
    assert(fixture.device->enableWrites == 0);
}

static void failed_initial_off_keeps_controls_locked()
{
    Fixture fixture;
    {
        std::lock_guard<std::mutex> lock(fixture.device->mutex);
        fixture.device->ignoreWrite = pps::REG_MODULE_ENABLE;
    }
    fixture.poll();
    eventually([&] { return fixture.controller->state().telemetry.lastError.find("Cannot confirm") != std::string::npos; });
    assert(fixture.device->enabled());
    assert(fixture.controller->state().output == OutputState::Unknown);
    assert(!fixture.controller->applySetpoints(400));
    assert(!fixture.controller->setOutputEnabled(true, 400));
}

int main()
{
    initial_connection_disables_output_and_rejects_wrong_identity();
    failure_preserves_last_good_but_marks_output_unknown_and_reconnects_safely();
    partial_and_missed_writes_cannot_enable_output();
    concurrent_edits_survive_apply_completion();
    emergency_off_interrupts_poll_without_waiting_on_caller();
    close_cancels_queued_enable_and_keeps_worker_resources_alive();
    failed_initial_off_keeps_controls_locked();
}
