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

namespace app_hardware_imu {

struct ImuSample : hal::HalBase::IMUData_t {
    bool hasSample = false;
    bool pending = false;
};

class ImuController {
public:
    using UpdateFn = std::function<void()>;
    using DataFn = std::function<hal::HalBase::IMUData_t()>;

    ImuController(UpdateFn updateFn, DataFn dataFn);

    ImuSample sample() const;
    ImuSample cached() const;

    static std::string formatAccel(float value);
    static std::string formatGyro(float value);

private:
    UpdateFn _updateFn;
    DataFn _dataFn;
    app_runtime::AsyncSample<ImuSample> _samples;
};

}  // namespace app_hardware_imu
