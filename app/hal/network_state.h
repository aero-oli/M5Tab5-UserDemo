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
        bool otaCancelRequested = false;
        bool accepting = true;
        bool stationWanted = false;
        uint64_t wifiGeneration = 0;
        uint32_t connectionStartedAtMs = 0;
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

    uint64_t beginStation(const std::string& ssid, uint32_t nowMs = 0)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        ++_state.wifiGeneration;
        _state.stationWanted = true;
        _state.connectionStartedAtMs = nowMs;
        _state.wifi.mode = HalBase::WifiMode::Station;
        _state.wifi.state = HalBase::WifiConnectionState::Connecting;
        _state.wifi.ssid = ssid;
        _state.wifi.ipAddress.clear();
        _state.wifi.lastError.clear();
        return _state.wifiGeneration;
    }
    bool expireStation(uint32_t nowMs, uint32_t timeoutMs)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_state.stationWanted || _state.wifi.state != HalBase::WifiConnectionState::Connecting ||
            nowMs - _state.connectionStartedAtMs < timeoutMs) return false;
        _state.stationWanted = false;
        ++_state.wifiGeneration;
        _state.wifi.state = HalBase::WifiConnectionState::Failed;
        _state.wifi.ipAddress.clear();
        _state.wifi.lastError = "Wi-Fi connection/DHCP timed out";
        return true;
    }
    void stopStationIntent()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _state.stationWanted = false;
        ++_state.wifiGeneration;
    }
    bool stationConnected(uint64_t generation, const std::string& ssid, const std::string& ip, int rssi)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!stationMatches(generation, ssid)) return false;
        _state.wifi.state = HalBase::WifiConnectionState::Connected;
        _state.wifi.ipAddress = ip;
        _state.wifi.rssi = rssi;
        _state.wifi.lastError.clear();
        return true;
    }
    bool stationDisconnected(uint64_t generation, const std::string& ssid, const std::string& error)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!stationMatches(generation, ssid)) return false;
        _state.wifi.state = HalBase::WifiConnectionState::Failed;
        _state.wifi.ipAddress.clear();
        _state.wifi.lastError = error;
        return true;
    }
    uint64_t beginScan()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state.wifi.mode == HalBase::WifiMode::Off) _state.wifi.mode = HalBase::WifiMode::Station;
        if (_state.wifi.state != HalBase::WifiConnectionState::Connected)
            _state.wifi.state = HalBase::WifiConnectionState::Scanning;
        return _state.wifiGeneration;
    }
    void finishScan(uint64_t generation, bool success)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_state.wifiGeneration == generation && _state.wifi.state == HalBase::WifiConnectionState::Scanning)
            _state.wifi.state = success ? HalBase::WifiConnectionState::Idle : HalBase::WifiConnectionState::Failed;
    }
    bool allowsCommands() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _state.accepting && !_state.otaActive;
    }
    bool reserveOta()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_state.accepting || _state.otaActive || _state.ota.rebootRequired ||
            _state.wifi.state != HalBase::WifiConnectionState::Connected) return false;
        _state.ota = {};
        _state.ota.state = HalBase::OtaState::Checking;
        _state.ota.canCancel = true;
        _state.otaActive = true;
        _state.otaCancelRequested = false;
        return true;
    }
    bool otaCancelled() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _state.otaCancelRequested;
    }
    bool enterOtaCommit()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_state.otaActive || _state.otaCancelRequested || !_state.accepting) return false;
        _state.ota.canCancel = false;
        _state.ota.state = HalBase::OtaState::Installing;
        return true;
    }
    void cancelOta()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        cancelOtaLocked();
    }
    void quiesce()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _state.accepting = false;
        cancelOtaLocked();
    }
    void resume()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _state.accepting = true;
    }

private:
    bool stationMatches(uint64_t generation, const std::string& ssid) const
    {
        return _state.stationWanted && _state.wifi.mode == HalBase::WifiMode::Station &&
               _state.wifiGeneration == generation && _state.wifi.ssid == ssid;
    }
    void cancelOtaLocked()
    {
        if (_state.otaActive && _state.ota.canCancel) {
            _state.otaCancelRequested = true;
            _state.ota.state = HalBase::OtaState::Cancelling;
        }
    }
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
