/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "imu_controller.h"

#include <cstdio>
#include <utility>

using namespace app_hardware_imu;

ImuController::ImuController(UpdateFn updateFn, DataFn dataFn)
    : _updateFn(std::move(updateFn)), _dataFn(std::move(dataFn))
{
}

ImuSample ImuController::sample() const
{
    const auto update = _updateFn;
    const auto dataFn = _dataFn;
    if (!update || !dataFn) {
        return cached();
    }
    return _samples.request("IMU sample", [update, dataFn] {
        update();
        ImuSample value;
        static_cast<hal::HalBase::IMUData_t&>(value) = dataFn();
        return value;
    });
}

ImuSample ImuController::cached() const
{
    return _samples.cached();
}

std::string ImuController::statusText(const ImuSample& sample, uint32_t nowMs)
{
    const auto source = sample.simulated ? "Simulated" : "Hardware";
    if (!sample.hasSample)
        return sample.error.empty() ? "Waiting for first IMU sample..." : "Unavailable: " + sample.error;
    if (!sample.valid || nowMs - sample.sampledAtMs > 2000)
        return std::string(source) + " - stale" + (sample.error.empty() ? "" : ": " + sample.error);
    return std::string(source) + " - up to 4 Hz while open";
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
