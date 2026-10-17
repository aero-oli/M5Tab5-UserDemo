/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
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
    uint64_t maxRenderUs = 0;
    uint64_t lastRenderUs = 0;
    uint64_t maxLockWaitUs = 0;
    uint64_t maxLockHoldUs = 0;
    uint64_t maxInputToFlushUs = 0;
    uint64_t p95InputToFlushUs = 0;
    uint32_t inputSamples = 0;
    uint32_t renderedFrames = 0;
    uint32_t freeHeap = 0;
    uint32_t minimumHeap = 0;
    uint32_t uiStackHeadroom = 0;
    uint32_t taskCount = 0;
};

class PerformanceTelemetry {
public:
    explicit PerformanceTelemetry(uint32_t slowThresholdMs = 50, size_t averageWindow = 16);

    void recordAppUpdate(uint32_t startedMs, uint32_t finishedMs);
    void recordLvglHandler(uint32_t startedMs, uint32_t finishedMs);
    void recordBackgroundJob(bool busy, const std::string& jobName);
    void recordRender(uint64_t startedUs, uint64_t finishedUs);
    void recordLock(uint64_t waitUs, uint64_t holdUs);
    void recordInput(uint64_t nowUs);
    void recordFlush(uint64_t nowUs);
    void recordResources(uint32_t freeHeap, uint32_t minimumHeap, uint32_t uiStackHeadroom, uint32_t taskCount);
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
    std::array<uint64_t, 128> _inputLatency{};
    size_t _inputCount = 0;
    size_t _inputNext = 0;
    uint64_t _inputStartedUs = 0;
    bool _inputPending = false;
};

PerformanceTelemetry& globalPerformanceTelemetry();
uint64_t monotonicMicros();

}  // namespace app_runtime
