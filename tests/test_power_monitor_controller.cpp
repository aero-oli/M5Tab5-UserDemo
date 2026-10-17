#include <cassert>
#include <cmath>
#include <chrono>
#include <future>
#include <thread>
#include <memory>

#include <apps/app_hardware_power_monitor/power_monitor_controller.h>

namespace {

template <typename Controller>
auto waitForSample(Controller& controller)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!controller.cached().hasSample) {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    return controller.cached();
}

void test_sample_updates_and_caches_readings()
{
    hal::HalBase::PMData_t data;
    data.valid = data.hasSample = true;
    data.busVoltage = 12.25f;
    data.busPower = 18.5f;
    data.shuntCurrent = 1.51f;
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    app_hardware_power_monitor::PowerMonitorController controller(
        [&entered, gate] { entered.set_value(); gate.wait(); },
        [data] { return data; }, [](int& value) { value = 46; return true; });

    auto request = std::async(std::launch::async, [&] { return controller.sample(); });
    assert(request.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready);
    const auto first = request.get();
    assert(!first.hasSample && first.pending);
    assert(controller.formatVoltage(first) == "--");
    assert(entered.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    auto requests = std::async(std::launch::async, [&] {
        for (int i = 0; i < 100; ++i) {
            const auto waiting = controller.sample();
            assert(!waiting.hasSample && waiting.pending);
        }
    });
    assert(requests.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready);
    requests.get();
    release.set_value();
    const auto sample = waitForSample(controller);
    assert(std::fabs(sample.busVoltage - 12.25f) < 0.001f);
    assert(std::fabs(sample.current - 1.51f) < 0.001f);
    assert(std::fabs(sample.outputPower - 18.5f) < 0.001f);
    assert(sample.cpuTempC == 46);
}

void test_cached_sample_survives_blocked_refresh_and_destruction()
{
    auto count = std::make_shared<int>(0);
    std::promise<void> entered, release, readDone;
    auto gate = release.get_future().share();
    auto controller = std::make_unique<app_hardware_power_monitor::PowerMonitorController>(
        [count, &entered, gate] { if (++*count == 2) { entered.set_value(); gate.wait(); } },
        [count, &readDone] {
            hal::HalBase::PMData_t data;
            data.valid = data.hasSample = true;
            data.busVoltage = 8.0f;
            data.shuntCurrent = -0.5f;
            if (*count == 2) { readDone.set_value(); }
            return data;
        }, [](int& value) { value = 42; return true; });
    controller->sample();
    assert(waitForSample(*controller).outputPower == 0.0f); // Keep the measured zero; do not fabricate an estimate.
    // Publication can precede the runner's completion bookkeeping.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    auto started = entered.get_future();
    while (started.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        assert(std::chrono::steady_clock::now() < deadline);
        controller->sample();
        std::this_thread::yield();
    }
    auto close = std::async(std::launch::async, [&] {
        const auto cached = controller->sample();
        assert(cached.hasSample && cached.pending && cached.busVoltage == 8.0f);
        controller.reset();
    });
    assert(close.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready);
    close.get();
    release.set_value();
    assert(readDone.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready);
}

struct SensorHal : hal::HalBase {
    using HalBase::publishPowerMonitorData;
    using HalBase::publishPowerMonitorError;
};

void test_successful_zero_failure_and_stale_health()
{
    SensorHal hardware;
    hardware.updatePowerMonitorData();
    auto data = hardware.getPowerMonitorData();
    assert(!data.valid && !data.hasSample && !data.error.empty());
    app_hardware_power_monitor::PowerMonitorController controller(
        [] {}, [&] { return hardware.getPowerMonitorData(); }, [](int&) { return false; });
    controller.sample();
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    assert(!controller.cached().hasSample);
    assert(controller.statusText(controller.cached(), 100).find("Unavailable") != std::string::npos);

    data = {};
    data.valid = data.hasSample = data.simulated = true;
    data.sampledAtMs = 100;
    hardware.publishPowerMonitorData(data);
    controller.sample();
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    auto sample = controller.cached();
    assert(sample.valid && sample.hasSample && sample.busVoltage == 0);
    assert(controller.formatVoltage(sample) == "0.00 V");
    assert(controller.formatCpuTemp(sample) == "--");
    assert(controller.statusText(sample, 100).find("Simulated") != std::string::npos);
    assert(controller.statusText(sample, 2101).find("stale") != std::string::npos);

    hardware.publishPowerMonitorError("Injected I2C failure");
    controller.sample();
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    sample = controller.cached();
    assert(!sample.valid && sample.hasSample && sample.sampledAtMs == 100 && sample.busVoltage == 0);
    assert(controller.statusText(sample, 101).find("stale: Injected I2C failure") != std::string::npos);
}

void test_formats_metric_values()
{
    app_hardware_power_monitor::PowerSample sample;
    sample.hasSample = sample.cpuTempValid = true;
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
    test_cached_sample_survives_blocked_refresh_and_destruction();
    test_formats_metric_values();
    test_successful_zero_failure_and_stale_health();
    assert(app_runtime::AsyncJobRunner::waitForAll(5000));
    return 0;
}
