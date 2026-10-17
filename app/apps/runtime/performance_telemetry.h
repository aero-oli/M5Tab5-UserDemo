/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace app_runtime {

struct PerformanceSnapshot {
    uint32_t totalAppUpdates = 0;
    uint32_t lastAppUpdateMs = 0;
    uint32_t avgAppUpdateMs = 0;
    uint32_t maxAppUpdateMs = 0;
    uint32_t slowAppUpdates = 0;
    uint32_t lastLvglMs = 0;
    uint32_t maxLvglMs = 0;
    bool backgroundBusy = false;
    std::string backgroundJob;
};

class PerformanceTelemetry {
public:
    explicit PerformanceTelemetry(uint32_t slowThresholdMs = 50, size_t averageWindow = 16);

    void recordAppUpdate(uint32_t startedMs, uint32_t finishedMs);
    void recordLvglHandler(uint32_t startedMs, uint32_t finishedMs);
    void recordBackgroundJob(bool busy, const std::string& jobName);
    PerformanceSnapshot snapshot() const;
    void reset();

private:
    static uint32_t elapsed(uint32_t startedMs, uint32_t finishedMs);
    uint32_t averageLocked() const;

    const uint32_t _slowThresholdMs;
    const size_t _averageWindow;
    mutable std::mutex _mutex;
    PerformanceSnapshot _snapshot;
    std::deque<uint32_t> _recentAppUpdates;
};

PerformanceTelemetry& globalPerformanceTelemetry();

}  // namespace app_runtime
