#include <cassert>
#include <cmath>

#include <apps/app_hardware_power_monitor/power_monitor_controller.h>

namespace {

void test_sample_updates_and_caches_readings()
{
    int updates = 0;
    hal::HalBase::PMData_t data;
    data.busVoltage = 12.25f;
    data.busPower = 18.5f;
    data.shuntCurrent = 1.51f;

    app_hardware_power_monitor::PowerMonitorController controller(
        [&] { ++updates; },
        [&] { return data; },
        [] { return 46; });

    auto sample = controller.sample();

    assert(updates == 1);
    assert(std::fabs(sample.busVoltage - 12.25f) < 0.001f);
    assert(std::fabs(sample.current - 1.51f) < 0.001f);
    assert(std::fabs(sample.outputPower - 18.5f) < 0.001f);
    assert(sample.cpuTempC == 46);
}

void test_sample_computes_power_when_hal_power_is_missing()
{
    hal::HalBase::PMData_t data;
    data.busVoltage = 8.0f;
    data.busPower = 0.0f;
    data.shuntCurrent = -0.5f;

    app_hardware_power_monitor::PowerMonitorController controller(
        [] {},
        [&] { return data; },
        [] { return 42; });

    auto sample = controller.sample();

    assert(std::fabs(sample.outputPower - 4.0f) < 0.001f);
}

void test_formats_metric_values()
{
    app_hardware_power_monitor::PowerSample sample;
    sample.busVoltage = 7.987f;
    sample.current = -0.5123f;
    sample.outputPower = 4.091f;
    sample.cpuTempC = 45;

    assert(app_hardware_power_monitor::PowerMonitorController::formatVoltage(sample) == "7.99 V");
    assert(app_hardware_power_monitor::PowerMonitorController::formatCurrent(sample) == "-0.512 A");
    assert(app_hardware_power_monitor::PowerMonitorController::formatPower(sample) == "4.09 W");
    assert(app_hardware_power_monitor::PowerMonitorController::formatCpuTemp(sample) == "45 C");
}

}  // namespace

int main()
{
    test_sample_updates_and_caches_readings();
    test_sample_computes_power_when_hal_power_is_missing();
    test_formats_metric_values();
    return 0;
}
