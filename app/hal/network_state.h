#pragma once

#include <atomic>
#include <mutex>
#include <hal/hal.h>

namespace hal {

// Publish compound status changes together; SDK calls belong outside update().
class NetworkState {
public:
    struct Snapshot {
        HalBase::WifiStatus wifi;
        HalBase::OtaStatus ota;
        bool otaActive = false;
    };

    Snapshot snapshot() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _state;
    }

    template <typename Update>
    void update(Update update)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        update(_state);
    }

private:
    mutable std::mutex _mutex;
    Snapshot _state;
};

// SDK commands may block. Reserve ownership without locking status readers or
// event callbacks, and reject an overlapping command rather than interleave it.
class NetworkCommandLease {
public:
    explicit NetworkCommandLease(std::atomic<bool>& busy) : _busy(busy)
    {
        bool expected = false;
        _acquired = _busy.compare_exchange_strong(expected, true);
    }
    ~NetworkCommandLease() { if (_acquired) _busy.store(false); }
    explicit operator bool() const { return _acquired; }
    NetworkCommandLease(const NetworkCommandLease&) = delete;
    NetworkCommandLease& operator=(const NetworkCommandLease&) = delete;
private:
    std::atomic<bool>& _busy;
    bool _acquired = false;
};

}  // namespace hal
