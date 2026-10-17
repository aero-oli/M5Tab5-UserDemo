/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "imu_controller.h"

#include <cstdio>
#include <utility>

using namespace app_hardware_imu;

ImuController::ImuController(UpdateFn updateFn, DataFn dataFn) : _updateFn(std::move(updateFn)), _dataFn(std::move(dataFn)) {}

ImuSample ImuController::sample() const
{
    if (_updateFn) {
        _updateFn();
    }
    if (_dataFn) {
        return _dataFn();
    }
    return {};
}

std::string ImuController::formatAccel(float value)
{
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%.3f g", value);
    return buffer;
}

std::string ImuController::formatGyro(float value)
{
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%.2f dps", value);
    return buffer;
}
