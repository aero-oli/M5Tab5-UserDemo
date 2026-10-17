/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "performance_telemetry.h"
#include <limits>

using namespace app_runtime;

PerformanceTelemetry::PerformanceTelemetry(uint32_t slowThresholdMs, size_t averageWindow)
    : _slowThresholdMs(slowThresholdMs), _averageWindow(averageWindow == 0 ? 1 : averageWindow)
{
}

void PerformanceTelemetry::recordAppUpdate(uint32_t startedMs, uint32_t finishedMs)
{
    const uint32_t duration = elapsed(startedMs, finishedMs);
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot.totalAppUpdates++;
    _snapshot.lastAppUpdateMs = duration;
    if (duration > _snapshot.maxAppUpdateMs) {
        _snapshot.maxAppUpdateMs = duration;
    }
    if (duration > _slowThresholdMs) {
        _snapshot.slowAppUpdates++;
    }

    _recentAppUpdates.push_back(duration);
    while (_recentAppUpdates.size() > _averageWindow) {
        _recentAppUpdates.pop_front();
    }
    _snapshot.avgAppUpdateMs = averageLocked();
}

void PerformanceTelemetry::recordLvglHandler(uint32_t startedMs, uint32_t finishedMs)
{
    const uint32_t duration = elapsed(startedMs, finishedMs);
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot.lastLvglMs = duration;
    if (duration > _snapshot.maxLvglMs) {
        _snapshot.maxLvglMs = duration;
    }
}

void PerformanceTelemetry::recordBackgroundJob(bool busy, const std::string& jobName)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot.backgroundBusy = busy;
    _snapshot.backgroundJob = busy ? jobName : "";
}

PerformanceSnapshot PerformanceTelemetry::snapshot() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _snapshot;
}

void PerformanceTelemetry::reset()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot = {};
    _recentAppUpdates.clear();
}

uint32_t PerformanceTelemetry::elapsed(uint32_t startedMs, uint32_t finishedMs)
{
    if (finishedMs >= startedMs) {
        return finishedMs - startedMs;
    }
    return (std::numeric_limits<uint32_t>::max() - startedMs) + finishedMs + 1;
}

uint32_t PerformanceTelemetry::averageLocked() const
{
    if (_recentAppUpdates.empty()) {
        return 0;
    }
    uint32_t total = 0;
    for (auto sample : _recentAppUpdates) {
        total += sample;
    }
    return total / static_cast<uint32_t>(_recentAppUpdates.size());
}

PerformanceTelemetry& app_runtime::globalPerformanceTelemetry()
{
    static PerformanceTelemetry telemetry;
    return telemetry;
}
