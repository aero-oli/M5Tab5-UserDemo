/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <ctime>
#include <functional>
#include <string>

namespace app_hardware_rtc {

struct RtcSample {
    tm time = {};
};

class RtcController {
public:
    using ReadRtcFn = std::function<void(tm*)>;

    explicit RtcController(ReadRtcFn readRtcFn);

    RtcSample sample() const;

    static std::string formatTime(const RtcSample& sample);
    static std::string formatDate(const RtcSample& sample);
    static std::string formatWeekday(const RtcSample& sample);

private:
    ReadRtcFn _readRtcFn;
};

}  // namespace app_hardware_rtc
