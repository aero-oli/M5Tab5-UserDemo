/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <string>
#include <hal/hal.h>

namespace app_hardware_imu {

using ImuSample = hal::HalBase::IMUData_t;

class ImuController {
public:
    using UpdateFn = std::function<void()>;
    using DataFn = std::function<hal::HalBase::IMUData_t()>;

    ImuController(UpdateFn updateFn, DataFn dataFn);

    ImuSample sample() const;

    static std::string formatAccel(float value);
    static std::string formatGyro(float value);

private:
    UpdateFn _updateFn;
    DataFn _dataFn;
};

}  // namespace app_hardware_imu
