/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace app_hardware_i2c {

class I2cScanController {
public:
    using ScanFn = std::function<std::vector<uint8_t>(bool internalBus)>;

    explicit I2cScanController(ScanFn scanFn);

    bool startScan(bool internalBus);
    void refresh();
    bool busy() const;
    std::string statusText() const;

    static std::string formatResult(bool internalBus, const std::vector<uint8_t>& devices);

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

}  // namespace app_hardware_i2c
