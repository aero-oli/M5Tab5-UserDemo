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
};

class ComMonitorController {
public:
    using DataFn = std::function<hal::HalBase::UartMonitorData_t&()>;
    using SendFn = std::function<void(const std::string&, bool)>;

    ComMonitorController(DataFn dataFn, SendFn sendFn);

    ComMonitorSample drainRx(size_t maxBytes = 800);
    void sendTestMessage();

private:
    DataFn _dataFn;
    SendFn _sendFn;
};

}  // namespace app_hardware_com_monitor
