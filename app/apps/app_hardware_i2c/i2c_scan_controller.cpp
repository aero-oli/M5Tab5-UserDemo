/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "i2c_scan_controller.h"
#include <cstdio>
#include <sstream>
#include <apps/runtime/async_job_runner.h>

using namespace app_hardware_i2c;

I2cScanController::I2cScanController(ScanFn scanFn) : _scanFn(std::move(scanFn))
{
}

bool I2cScanController::startScan(bool internalBus)
{
    if (!_scanFn) {
        std::lock_guard<std::mutex> lock(_mutex);
        _statusText = "I2C scan is unavailable.";
        _busy = false;
        return false;
    }

    auto state = std::make_shared<ScanState>();
    auto scanFn = _scanFn;
    if (!app_runtime::globalAsyncJobRunner().start(internalBus ? "internal I2C scan" : "port A I2C scan",
                                                   [state, scanFn, internalBus]() {
                                                       auto devices = scanFn(internalBus);
                                                       auto text = I2cScanController::formatResult(internalBus, devices);
                                                       std::lock_guard<std::mutex> lock(state->mutex);
                                                       state->text = std::move(text);
                                                       state->complete = true;
                                                   })) {
        std::lock_guard<std::mutex> lock(_mutex);
        _statusText = "Another background task is already running. Try again in a moment.";
        _busy = false;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _scanState = state;
        _statusText = internalBus ? "Scanning internal I2C bus..." : "Scanning Port A I2C bus...";
        _busy = true;
    }
    return true;
}

void I2cScanController::refresh()
{
    std::shared_ptr<ScanState> state;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        state = _scanState;
    }
    if (!state) {
        return;
    }

    std::string text;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->complete) {
            return;
        }
        text = state->text;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _statusText = std::move(text);
    _busy = false;
    _scanState.reset();
}

bool I2cScanController::busy() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _busy;
}

std::string I2cScanController::statusText() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _statusText;
}

std::string I2cScanController::formatResult(bool internalBus, const std::vector<uint8_t>& devices)
{
    std::ostringstream out;
    out << (internalBus ? "Internal bus" : "Port A") << " devices:";
    if (devices.empty()) {
        out << "\nNo devices found.";
        return out.str();
    }

    for (auto address : devices) {
        char line[16];
        std::snprintf(line, sizeof(line), "\n0x%02X", address);
        out << line;
    }
    return out.str();
}
