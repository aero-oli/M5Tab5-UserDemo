/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "performance_telemetry.h"
#include <limits>
#include <algorithm>
#include <chrono>

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
    auto result = _snapshot;
    if (_inputCount) {
        auto values = _inputLatency;
        std::sort(values.begin(), values.begin() + _inputCount);
        result.p95InputToFlushUs = values[(_inputCount * 95 + 99) / 100 - 1];
    }
    return result;
}

void PerformanceTelemetry::reset()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot = {};
    _recentAppUpdates.clear();
    _inputCount = _inputNext = 0;
    _inputPending = false;
}

void PerformanceTelemetry::recordRender(uint64_t startedUs, uint64_t finishedUs)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot.lastRenderUs = finishedUs - startedUs;
    _snapshot.maxRenderUs = std::max(_snapshot.maxRenderUs, _snapshot.lastRenderUs);
    ++_snapshot.renderedFrames;
}
void PerformanceTelemetry::recordLock(uint64_t waitUs, uint64_t holdUs)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot.maxLockWaitUs = std::max(_snapshot.maxLockWaitUs, waitUs);
    _snapshot.maxLockHoldUs = std::max(_snapshot.maxLockHoldUs, holdUs);
}
void PerformanceTelemetry::recordInput(uint64_t nowUs)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_inputPending) _inputStartedUs = nowUs;
    _inputPending = true;
}
void PerformanceTelemetry::recordFlush(uint64_t nowUs)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_inputPending) return;
    _inputPending = false;
    const auto latency = nowUs - _inputStartedUs;
    _inputLatency[_inputNext] = latency;
    _inputNext = (_inputNext + 1) % _inputLatency.size();
    _inputCount = std::min(_inputCount + 1, _inputLatency.size());
    ++_snapshot.inputSamples;
    _snapshot.maxInputToFlushUs = std::max(_snapshot.maxInputToFlushUs, latency);
}
void PerformanceTelemetry::recordResources(uint32_t freeHeap, uint32_t minimumHeap, uint32_t stack, uint32_t tasks)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot.freeHeap = freeHeap;
    _snapshot.minimumHeap = minimumHeap;
    _snapshot.uiStackHeadroom = stack;
    _snapshot.taskCount = tasks;
}
uint64_t app_runtime::monotonicMicros()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
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
