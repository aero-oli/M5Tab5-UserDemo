/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <string>
#include <hal/hal.h>
#include <apps/runtime/async_sample.h>

namespace app_hardware_power_monitor {

struct PowerSample {
    bool hasSample = false;
    bool pending = false;
    float busVoltage = 0.0f;
    float current = 0.0f;
    float outputPower = 0.0f;
    float shuntVoltage = 0.0f;
    int cpuTempC = 0;
};

class PowerMonitorController {
public:
    using UpdateFn = std::function<void()>;
    using DataFn = std::function<hal::HalBase::PMData_t()>;
    using CpuTempFn = std::function<int()>;

    PowerMonitorController(UpdateFn updateFn, DataFn dataFn, CpuTempFn cpuTempFn);

    PowerSample sample();
    PowerSample cached() const;

    static std::string formatVoltage(const PowerSample& sample);
    static std::string formatCurrent(const PowerSample& sample);
    static std::string formatPower(const PowerSample& sample);
    static std::string formatCpuTemp(const PowerSample& sample);

private:
    UpdateFn _updateFn;
    DataFn _dataFn;
    CpuTempFn _cpuTempFn;
    app_runtime::AsyncSample<PowerSample> _samples;
};

}  // namespace app_hardware_power_monitor
