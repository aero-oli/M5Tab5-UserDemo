/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "rtc_controller.h"

#include <cstdio>
#include <utility>

using namespace app_hardware_rtc;

RtcController::RtcController(ReadRtcFn readRtcFn) : _readRtcFn(std::move(readRtcFn)) {}

RtcSample RtcController::sample() const
{
    RtcSample result;
    if (_readRtcFn) {
        _readRtcFn(&result.time);
    }
    return result;
}

std::string RtcController::formatTime(const RtcSample& sample)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", sample.time.tm_hour, sample.time.tm_min, sample.time.tm_sec);
    return buffer;
}

std::string RtcController::formatDate(const RtcSample& sample)
{
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", sample.time.tm_year + 1900, sample.time.tm_mon + 1,
                  sample.time.tm_mday);
    return buffer;
}

std::string RtcController::formatWeekday(const RtcSample& sample)
{
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "Weekday: %d", sample.time.tm_wday);
    return buffer;
}
