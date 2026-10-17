#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <hal/hal.h>

namespace hal {

// One codec owner from reservation through cancellation/driver cleanup. State
// locks never span IO. The platform keeps this owner alive for its workers.
class AudioSession {
public:
    using Token = uint64_t;
    using Operation = HalBase::AudioOperation;
    using Phase = HalBase::AudioPhase;

    Token reserve(Operation operation, Phase phase)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_accepting || _status.busy) return 0;
        _status = {operation, phase, true, {}};
        _cancelled = false;
        return ++_token;
    }
    HalBase::AudioStatus snapshot() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _status;
    }
    bool cancelled(Token token) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return token != _token || _cancelled;
    }
    void cancel()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        cancelLocked();
    }
    void setPhase(Token token, Phase phase)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (token == _token && _status.busy && !_cancelled) _status.phase = phase;
    }
    void fail(Token token, const std::string& error)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (token == _token && _status.busy) {
            if (_status.lastError.empty()) _status.lastError = error;
            _status.phase = Phase::Failed;
        }
    }
    void finish(Token token)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (token != _token || !_status.busy) return;
        _status.busy = false;
        _status.phase = _status.lastError.empty() ? Phase::Idle : Phase::Failed;
        _changed.notify_all();
    }
    void startFailed(Token token)
    {
        fail(token, "Could not start audio worker");
        finish(token);
    }
    bool stopAndWait(uint32_t timeoutMs)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _accepting = false;
        cancelLocked();
        if (_changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return !_status.busy; }))
            return true;
        _status.lastError = "Audio did not stop before deadline";
        _status.phase = Phase::Failed;
        return false; // Keep ownership/resources until the real worker finishes.
    }
    void resume()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _accepting = true;
    }
private:
    void cancelLocked()
    {
        if (_status.busy) {
            _cancelled = true;
            if (_status.lastError.empty()) _status.phase = Phase::Cancelling;
        }
    }
    mutable std::mutex _mutex;
    std::condition_variable _changed;
    HalBase::AudioStatus _status;
    Token _token = 0;
    bool _cancelled = false;
    bool _accepting = true;
};

// Worker-owned cache. A zero volume must stay muted: this BSP's set_volume(0)
// mutes without programming a zero gain, so a later unconditional unmute leaks
// the previous audible gain. Apply changes at every output transfer boundary.
class AudioVolume {
public:
    void invalidate() { _applied = -1; }
    template <typename SetVolume, typename SetMute>
    bool apply(uint8_t value, SetVolume setVolume, SetMute setMute)
    {
        const bool muted = value == 0;
        if (_applied == value && _muted == muted) return true;
        if (!setVolume(value) || !setMute(muted)) { invalidate(); return false; }
        _applied = value;
        _muted = muted;
        return true;
    }
    template <typename SetMute>
    bool mute(SetMute setMute)
    {
        if (!setMute(true)) { invalidate(); return false; }
        _muted = true;
        return true;
    }
private:
    int _applied = -1;
    bool _muted = true;
};

// Platform IO must itself be bounded. Current Tab5 codec calls have a 1s
// driver timeout; a timeout_ms argument to its BSP wrapper is ignored.
// Returning false with cancellation is normal; other failures are published.
template <typename Io>
bool transferAudio(AudioSession& session, AudioSession::Token token, void* data, size_t bytes, Io io)
{
    auto* buffer = static_cast<uint8_t*>(data);
    size_t offset = 0;
    while (offset < bytes) {
        if (session.cancelled(token)) return false;
        const size_t chunk = std::min<size_t>(4096, bytes - offset);
        size_t done = 0;
        const bool ok = io(buffer + offset, chunk, done);
        if (session.cancelled(token)) return false;
        if (!ok || done == 0 || done > chunk || done % sizeof(int16_t)) {
            session.fail(token, "Audio driver transfer failed");
            return false;
        }
        offset += done;
    }
    return !session.cancelled(token);
}

}  // namespace hal
