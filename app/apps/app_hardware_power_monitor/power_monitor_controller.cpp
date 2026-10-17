/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "power_monitor_controller.h"

#include <cmath>
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
        value.busVoltage = data.busVoltage;
        value.current = data.shuntCurrent;
        value.shuntVoltage = data.shuntVoltage;
        value.outputPower = std::fabs(data.busPower) > 0.001f ? data.busPower : data.busVoltage * std::fabs(data.shuntCurrent);
        value.cpuTempC = cpuTemp();
        return value;
    });
}

PowerSample PowerMonitorController::cached() const
{
    return _samples.cached();
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
    if (!sample.hasSample) {
        return "--";
    }
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%d C", sample.cpuTempC);
    return buffer;
}
