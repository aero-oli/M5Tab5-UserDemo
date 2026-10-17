/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "sd_card_controller.h"
#include <sstream>
#include <apps/runtime/async_job_runner.h>

using namespace app_hardware_sd;

SdCardController::SdCardController(MountedFn mountedFn, ScanFn scanFn)
    : _mountedFn(std::move(mountedFn)), _scanFn(std::move(scanFn))
{
}

bool SdCardController::startScan()
{
    if (!_mountedFn || !_scanFn) {
        std::lock_guard<std::mutex> lock(_mutex);
        _statusText = "SD card scan is unavailable.";
        _busy = false;
        return false;
    }

    auto state = std::make_shared<ScanState>();
    auto mountedFn = _mountedFn;
    auto scanFn = _scanFn;
    if (!app_runtime::globalAsyncJobRunner().start("SD card scan", [state, mountedFn, scanFn]() {
            const bool mounted = mountedFn();
            auto files = mounted ? scanFn("/") : std::vector<hal::HalBase::FileEntry_t> {};
            auto text = SdCardController::formatResult(mounted, files);
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
        _statusText = "Scanning SD card...";
        _busy = true;
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
    _statusText = std::move(text);
    _busy = false;
    _scanState.reset();
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

std::string SdCardController::formatResult(bool mounted, const std::vector<hal::HalBase::FileEntry_t>& files)
{
    if (!mounted) {
        return "No SD card mounted.";
    }
    if (files.empty()) {
        return "SD card is mounted but no files were found.";
    }

    std::ostringstream out;
    out << "SD card root:";
    for (const auto& file : files) {
        out << "\n" << (file.isDir ? "[DIR] " : "      ") << file.name;
    }
    return out.str();
}
