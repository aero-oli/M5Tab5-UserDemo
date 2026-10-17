/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "com_monitor_controller.h"

#include <mutex>
#include <utility>

using namespace app_hardware_com_monitor;

ComMonitorController::ComMonitorController(DataFn dataFn, SendFn sendFn) : _dataFn(std::move(dataFn)), _sendFn(std::move(sendFn)) {}

ComMonitorSample ComMonitorController::drainRx(size_t maxBytes)
{
    ComMonitorSample sample;
    if (!_dataFn || maxBytes == 0) {
        return sample;
    }
    auto& data = _dataFn();
    std::lock_guard<std::mutex> lock(data.mutex);
    while (!data.rxQueue.empty() && sample.text.size() < maxBytes) {
        sample.text.push_back(static_cast<char>(data.rxQueue.front()));
        data.rxQueue.pop();
    }
    sample.bytesRead = sample.text.size();
    return sample;
}

void ComMonitorController::sendTestMessage()
{
    if (_sendFn) {
        _sendFn("hello from Tab5", true);
    }
}
