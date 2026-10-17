#include <cassert>
#include <string>
#include <vector>

#include <apps/app_hardware_switches/switches_controller.h>

namespace {

struct ToggleCall {
    app_hardware_switches::SwitchId id;
    bool enabled;
};

void test_reads_initial_toggle_state()
{
    app_hardware_switches::SwitchSnapshot snapshot;
    snapshot.charge = true;
    snapshot.chargeQc = false;
    snapshot.ext5v = true;
    snapshot.usbA5v = false;
    snapshot.extAntenna = true;

    app_hardware_switches::SwitchesController controller(
        [&] { return snapshot; },
        [&](app_hardware_switches::SwitchId, bool) {},
        [] { return app_hardware_switches::InterfaceSnapshot{}; });

    auto read = controller.readSwitches();

    assert(read.charge);
    assert(!read.chargeQc);
    assert(read.ext5v);
    assert(!read.usbA5v);
    assert(read.extAntenna);
}

void test_set_switch_dispatches_to_hal_binding()
{
    std::vector<ToggleCall> calls;
    app_hardware_switches::SwitchesController controller(
        [] { return app_hardware_switches::SwitchSnapshot{}; },
        [&](app_hardware_switches::SwitchId id, bool enabled) { calls.push_back({id, enabled}); },
        [] { return app_hardware_switches::InterfaceSnapshot{}; });

    controller.setSwitch(app_hardware_switches::SwitchId::UsbA5v, true);
    controller.setSwitch(app_hardware_switches::SwitchId::ChargeQc, false);

    assert(calls.size() == 2);
    assert(calls[0].id == app_hardware_switches::SwitchId::UsbA5v);
    assert(calls[0].enabled);
    assert(calls[1].id == app_hardware_switches::SwitchId::ChargeQc);
    assert(!calls[1].enabled);
}

void test_formats_interface_status()
{
    app_hardware_switches::InterfaceSnapshot snapshot;
    snapshot.valid = true;
    snapshot.hasSample = true;
    snapshot.usbCKnown = true;
    snapshot.headphoneKnown = true;
    snapshot.usbC = true;
    snapshot.usbA = false;
    snapshot.headphone = true;

    assert(app_hardware_switches::SwitchesController::formatInterfaceStatus(snapshot) ==
           "USB-C: connected    USB-A: unknown    Headphone: connected");
    snapshot.valid = false;
    snapshot.error = "Read failed";
    assert(app_hardware_switches::SwitchesController::formatInterfaceStatus(snapshot) ==
           "USB-C: unknown    USB-A: unknown    Headphone: unknown (stale)");
    snapshot.valid = true;
    snapshot.simulated = true;
    assert(app_hardware_switches::SwitchesController::formatInterfaceStatus(snapshot).find("Simulated: ") == 0);
}

}  // namespace

int main()
{
    test_reads_initial_toggle_state();
    test_set_switch_dispatches_to_hal_binding();
    test_formats_interface_status();
    return 0;
}
