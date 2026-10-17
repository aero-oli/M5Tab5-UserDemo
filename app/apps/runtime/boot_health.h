#pragma once
#include <cstdint>

namespace app_runtime {
// Local startup diagnostics only: this says nothing about optional peripherals,
// radio connectivity, electrical behavior, or the installed bootloader binary.
class BootHealthGate {
public:
    enum class Decision { Waiting, Accept, Reject };
    BootHealthGate(uint32_t nowMs, uint32_t updates, uint32_t renders)
        : _started(nowMs),
          _windowStarted(nowMs),
          _lastUpdateMs(nowMs),
          _lastRenderMs(nowMs),
          _initialUpdates(updates),
          _initialRenders(renders),
          _updates(updates),
          _renders(renders)
    {
    }
    Decision observe(uint32_t nowMs, uint32_t updates, uint32_t renders, bool coreReady)
    {
        // Diagnostics can reset telemetry. Never treat a reset as billions of
        // successful updates through unsigned counter subtraction.
        if (!coreReady || updates < _updates || renders < _renders) {
            _windowStarted  = nowMs;
            _initialUpdates = updates;
            _initialRenders = renders;
        }
        if (updates != _updates) {
            _updates      = updates;
            _lastUpdateMs = nowMs;
        }
        if (renders != _renders) {
            _renders      = renders;
            _lastRenderMs = nowMs;
        }
        const uint32_t elapsed = nowMs - _started;
        if (elapsed >= 10000) return Decision::Reject;
        // At least 3 seconds, 60 lifecycle transitions and multiple independently
        // scheduled probe renders; a first frame alone must never confirm an image.
        if (coreReady && nowMs - _windowStarted >= 3000 && updates - _initialUpdates >= 60 &&
            renders - _initialRenders >= 4 && nowMs - _lastUpdateMs <= 500 && nowMs - _lastRenderMs <= 1000)
            return Decision::Accept;
        return Decision::Waiting;
    }

private:
    uint32_t _started, _windowStarted, _lastUpdateMs, _lastRenderMs;
    uint32_t _initialUpdates, _initialRenders, _updates, _renders;
};
}  // namespace app_runtime
