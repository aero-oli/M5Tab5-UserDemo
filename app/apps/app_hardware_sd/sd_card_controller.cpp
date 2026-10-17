/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "sd_card_controller.h"
#include <sstream>
#include <apps/runtime/async_job_runner.h>

using namespace app_hardware_sd;

SdCardController::SdCardController(ScanFn scanFn) : _scanFn(std::move(scanFn))
{
}

bool SdCardController::startScan()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_busy) return false;
        if (!_scanFn) {
            _statusText = "SD card scan is unavailable.";
            return false;
        }
        _busy = true;  // Reserve locally before another caller can submit a scan.
    }

    auto state  = std::make_shared<ScanState>();
    auto scanFn = _scanFn;
    if (!app_runtime::globalAsyncJobRunner().start("SD card scan", [state, scanFn]() {
            auto text = SdCardController::formatResult(scanFn("/"));
            std::lock_guard<std::mutex> lock(state->mutex);
            state->text     = std::move(text);
            state->complete = true;
        })) {
        std::lock_guard<std::mutex> lock(_mutex);
        _statusText = "Another background task is already running. Try again in a moment.";
        _busy       = false;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _scanState  = state;
        _statusText = "Scanning SD card...";
        _busy       = true;
    }
    return true;
}

void SdCardController::refresh()
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
    if (_scanState == state) {
        _statusText = std::move(text);
        _busy       = false;
        _scanState.reset();
    }
}

bool SdCardController::busy() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _busy;
}

std::string SdCardController::statusText() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _statusText;
}

std::string SdCardController::formatResult(const hal::HalBase::SdScanResult& result)
{
    using Status             = hal::HalBase::SdScanStatus;
    const std::string source = result.simulated ? "Desktop simulation: " : "";
    switch (result.status) {
        case Status::MountFailed:
            return source + "SD card mount failed. " + result.error;
        case Status::ReadFailed:
            return source + "SD card scan failed. " + result.error;
        case Status::Unavailable:
            return source + "SD card scan unavailable. " + result.error;
        case Status::Success:
            break;
    }
    if (result.entries.empty()) return source + "SD card root is empty.";
    std::ostringstream out;
    out << source << "SD card root:";
    for (const auto& file : result.entries) out << "\n" << (file.isDir ? "[DIR] " : "      ") << file.name;
    if (result.truncated) out << "\nShowing first " << hal::HalBase::kMaxSdEntries << " entries.";
    return out.str();
}
