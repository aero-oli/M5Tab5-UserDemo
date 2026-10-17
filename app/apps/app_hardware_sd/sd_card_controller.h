/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <hal/hal.h>

namespace app_hardware_sd {

class SdCardController {
public:
    using ScanFn = std::function<hal::HalBase::SdScanResult(const std::string& path)>;

    explicit SdCardController(ScanFn scanFn);

    bool startScan();
    void refresh();
    bool busy() const;
    std::string statusText() const;

    static std::string formatResult(const hal::HalBase::SdScanResult& result);

private:
    struct ScanState {
        std::mutex mutex;
        bool complete = false;
        std::string text;
    };

    ScanFn _scanFn;
    std::shared_ptr<ScanState> _scanState;
    mutable std::mutex _mutex;
    std::string _statusText = "No scan run yet.";
    bool _busy              = false;
};

}  // namespace app_hardware_sd
