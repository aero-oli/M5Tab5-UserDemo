/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "com_monitor_controller.h"

#include <mutex>
#include <utility>

using namespace app_hardware_com_monitor;

ComMonitorController::ComMonitorController(DataFn dataFn, SendFn sendFn)
    : _dataFn(std::move(dataFn)), _sendFn(std::move(sendFn))
{
}

ComMonitorSample ComMonitorController::drainRx(size_t maxBytes)
{
    ComMonitorSample sample;
    if (!_dataFn || maxBytes == 0) {
        return sample;
    }
    auto& data = _dataFn();
    {
        std::lock_guard<std::mutex> lock(data.mutex);
        sample.simulated = data.simulated;
        sample.error     = data.error;
        sample.dropped   = data.rxDropped;
        while (!data.rxQueue.empty() && sample.text.size() < std::min(maxBytes, hal::UartMonitorState::kCapacity)) {
            sample.text.push_back(static_cast<char>(data.rxQueue.front()));
            data.rxQueue.pop();
        }
    }
    sample.bytesRead = sample.text.size();
    _logText += sample.text;
    if (_logText.size() > kLogBytes) _logText.erase(0, _logText.size() - kLogBytes);
    return sample;
}

bool ComMonitorController::sendTestMessage()
{
    return _sendFn && _sendFn("hello from Tab5", true);
}
