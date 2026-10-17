#include <cassert>
#include <cstdint>
#include <vector>
#include <apps/app_hardware_gpio/gpio_controller.h>
#include <hal/profiles/tab5_gpio.h>

using app_hardware_gpio::GpioController;

namespace {
struct Call {
    enum Kind { Init, Set, Reset } kind;
    uint8_t pin;
    bool level;
};

struct FakeGpio {
    hal::GpioClaimResult initResult = hal::GpioClaimResult::Claimed;
    bool setOk                      = true;
    int failedRelease               = -1;
    std::vector<Call> calls;

    GpioController controller()
    {
        return GpioController(
            hal::tab5ExpansionPins(),
            [this](uint8_t pin) {
                calls.push_back({Call::Init, pin, false});
                return initResult;
            },
            [this](uint8_t pin, bool high) {
                calls.push_back({Call::Set, pin, high});
                return setOk;
            },
            [this](uint8_t pin) {
                calls.push_back({Call::Reset, pin, false});
                return pin != failedRelease;
            });
    }
};

void inventoryAndOpening()
{
    const auto inventory = hal::tab5ExpansionPins();
    assert(inventory.size() == 24);
    assert(inventory.front().pin == 49 && inventory.back().pin == 51);
    assert(!hal::gpioOutputAllowed(inventory, 53));
    assert(!hal::gpioOutputAllowed(inventory, 54));
    for (auto pin : {0, 1, 20, 21, 31, 32, 34, 36, 39, 40, 41, 42, 43, 44, 99}) {
        assert(!hal::gpioOutputAllowed(inventory, pin));
    }
    FakeGpio gpio;
    {
        auto controller = gpio.controller();
        assert(controller.claimedCount() == 0);
        assert(controller.resetAll());
        for (auto pin : {0, 1, 53, 54, 99}) {
            assert(!controller.claim(pin));
            assert(!controller.setLevel(pin, true));
            assert(!controller.release(pin));
        }
        assert(!controller.setLevel(49, true));
        assert(gpio.calls.empty());
    }
    assert(gpio.calls.empty());
}

void explicitClaimAndRelease()
{
    FakeGpio gpio;
    auto controller = gpio.controller();
    assert(controller.claim(49));
    assert(controller.state(49)->claimed && controller.state(49)->levelKnown && !controller.state(49)->high);
    assert(!controller.claim(49));
    assert(controller.setLevel(49, false));
    assert(gpio.calls.size() == 1);
    assert(controller.setLevel(49, true));
    assert(controller.state(49)->high && gpio.calls.size() == 2);
    assert(gpio.calls.back().kind == Call::Set && gpio.calls.back().level);
    assert(controller.setLevel(49, true));
    assert(gpio.calls.size() == 2);
    assert(controller.release(49));
    assert(!controller.state(49)->claimed && !controller.state(49)->levelKnown);
    assert(gpio.calls.back().kind == Call::Reset);
    assert(controller.resetAll());
    assert(gpio.calls.size() == 3);
}

void claimFailures()
{
    FakeGpio gpio;
    {
        auto controller = gpio.controller();
        gpio.initResult = hal::GpioClaimResult::Rejected;
        assert(!controller.claim(49));
        assert(!controller.state(49)->claimed);
        assert(controller.resetAll());
        assert(gpio.calls.size() == 1);
        gpio.initResult = hal::GpioClaimResult::Unknown;
        assert(!controller.claim(49));
        assert(controller.state(49)->claimed && !controller.state(49)->levelKnown);
        assert(!controller.lastError().empty());
        assert(!controller.setLevel(49, true));
        assert(gpio.calls.size() == 2);
    }
    assert(gpio.calls.size() == 3 && gpio.calls.back().kind == Call::Reset);
}

void unknownStateAndRetry()
{
    FakeGpio gpio;
    auto controller = gpio.controller();
    assert(controller.claim(49));
    assert(controller.claim(50));
    gpio.setOk = false;
    assert(!controller.setLevel(49, true));
    assert(controller.state(49)->claimed && !controller.state(49)->levelKnown);
    const auto calls = gpio.calls.size();
    assert(!controller.setLevel(49, false));
    assert(gpio.calls.size() == calls);
    gpio.failedRelease = 49;
    assert(!controller.resetAll());
    assert(controller.state(49)->claimed && !controller.state(50)->claimed);
    assert(!controller.lastError().empty());
    assert(controller.claimedCount() == 1);
    gpio.failedRelease = -1;
    assert(controller.resetAll());
    assert(controller.claimedCount() == 0 && controller.lastError().empty());
    gpio.setOk = true;
    assert(controller.claim(49));
    assert(controller.setLevel(49, true));
}

void teardownOnlyReleasesOwnedPins()
{
    FakeGpio gpio;
    {
        auto controller = gpio.controller();
        assert(controller.claim(49));
        assert(controller.claim(51));
        assert(controller.setLevel(51, true));
    }
    assert(gpio.calls.size() == 5);
    assert(gpio.calls[3].kind == Call::Reset && gpio.calls[3].pin == 49);
    assert(gpio.calls[4].kind == Call::Reset && gpio.calls[4].pin == 51);
}

void duplicateInventory()
{
    GpioController controller({{49, "Expansion", true, ""}, {49, "Duplicate", true, ""}}, {}, {}, {});
    assert(controller.pins().size() == 1);
    assert(!controller.claim(49));
}
}  // namespace

int main()
{
    inventoryAndOpening();
    explicitClaimAndRelease();
    claimFailures();
    unknownStateAndRetry();
    teardownOnlyReleasesOwnedPins();
    duplicateInventory();
}
