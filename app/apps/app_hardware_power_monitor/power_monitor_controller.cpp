/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "power_monitor_controller.h"

#include <cstdio>

using namespace app_hardware_power_monitor;

namespace {

std::string format(const char* fmt, float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), fmt, value);
    return buffer;
}

}  // namespace

PowerMonitorController::PowerMonitorController(UpdateFn updateFn, DataFn dataFn, CpuTempFn cpuTempFn)
    : _updateFn(std::move(updateFn)), _dataFn(std::move(dataFn)), _cpuTempFn(std::move(cpuTempFn))
{
}

PowerSample PowerMonitorController::sample()
{
    const auto update = _updateFn;
    const auto dataFn = _dataFn;
    const auto cpuTemp = _cpuTempFn;
    if (!update || !dataFn || !cpuTemp) {
        return cached();
    }
    return _samples.request("power monitor sample", [update, dataFn, cpuTemp] {
        update();
        const auto data = dataFn();
        PowerSample value;
        value.valid = data.valid;
        value.hasSample = data.hasSample;
        value.simulated = data.simulated;
        value.sampledAtMs = data.sampledAtMs;
        value.error = data.error;
        value.busVoltage = data.busVoltage;
        value.current = data.shuntCurrent;
        value.shuntVoltage = data.shuntVoltage;
        value.outputPower = data.busPower;
        value.cpuTempValid = cpuTemp(value.cpuTempC);
        return value;
    });
}

PowerSample PowerMonitorController::cached() const
{
    return _samples.cached();
}

std::string PowerMonitorController::statusText(const PowerSample& sample, uint32_t nowMs)
{
    const auto source = sample.simulated ? "Simulated" : "Hardware";
    if (!sample.hasSample) return sample.error.empty() ? "Waiting for first power sample..." : "Unavailable: " + sample.error;
    if (!sample.valid || nowMs - sample.sampledAtMs > 2000)
        return std::string(source) + " - stale" + (sample.error.empty() ? "" : ": " + sample.error);
    return std::string(source) + " - up to 4 Hz while open";
}

std::string PowerMonitorController::formatVoltage(const PowerSample& sample)
{
    if (!sample.hasSample) {
        return "--";
    }
    return format("%.2f V", sample.busVoltage);
}

std::string PowerMonitorController::formatCurrent(const PowerSample& sample)
{
    if (!sample.hasSample) {
        return "--";
    }
    return format("%.3f A", sample.current);
}

std::string PowerMonitorController::formatPower(const PowerSample& sample)
{
    if (!sample.hasSample) {
        return "--";
    }
    return format("%.2f W", sample.outputPower);
}

std::string PowerMonitorController::formatCpuTemp(const PowerSample& sample)
{
    if (!sample.cpuTempValid) {
        return "--";
    }
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%d C", sample.cpuTempC);
    return buffer;
}
