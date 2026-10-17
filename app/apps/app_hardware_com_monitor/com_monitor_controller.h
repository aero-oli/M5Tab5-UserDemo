/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <hal/hal.h>

namespace app_hardware_com_monitor {

struct ComMonitorSample {
    std::string text;
    size_t bytesRead = 0;
    size_t dropped = 0;
    bool simulated = false;
    std::string error;
};

class ComMonitorController {
public:
    using DataFn = std::function<hal::HalBase::UartMonitorData_t&()>;
    using SendFn = std::function<bool(const std::string&, bool)>;

    ComMonitorController(DataFn dataFn, SendFn sendFn);

    ComMonitorSample drainRx(size_t maxBytes = 800);
    bool sendTestMessage();
    const std::string& logText() const { return _logText; }
    static constexpr size_t kLogBytes = 2000;

private:
    DataFn _dataFn;
    SendFn _sendFn;
    std::string _logText;
};

}  // namespace app_hardware_com_monitor
