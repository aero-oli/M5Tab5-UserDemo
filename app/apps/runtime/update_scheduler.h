/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace app_runtime {

class UpdateScheduler {
public:
    explicit UpdateScheduler(uint32_t minIntervalMs) : _minIntervalMs(minIntervalMs) {}

    bool shouldRun(uint32_t nowMs)
    {
        if (_minIntervalMs == 0) {
            _lastRunMs = nowMs;
            _hasRun = true;
            return true;
        }
        if (!_hasRun || static_cast<uint32_t>(nowMs - _lastRunMs) >= _minIntervalMs) {
            _lastRunMs = nowMs;
            _hasRun = true;
            return true;
        }
        return false;
    }

private:
    uint32_t _minIntervalMs = 0;
    uint32_t _lastRunMs = 0;
    bool _hasRun = false;
};

}  // namespace app_runtime
