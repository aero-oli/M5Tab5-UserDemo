#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <utility>

namespace hal {

// Queue storage survives its HAL owner until the UART worker has really exited.
struct UartMonitorState {
    static constexpr size_t kCapacity = 4096;
    mutable std::mutex mutex;
    std::queue<uint8_t> rxQueue, txQueue;
    std::condition_variable changed;
    std::atomic<bool> stopRequested {false};
    bool accepting = true, workerRunning = false, driverActive = false, simulated = false;
    size_t rxDropped = 0;
    std::string error;

    bool queueTx(const std::string& message, bool newline)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!accepting || txQueue.size() > kCapacity || message.size() > kCapacity - (newline ? 1 : 0) ||
            message.size() + (newline ? 1 : 0) > kCapacity - txQueue.size()) return false;
        for (const auto byte : message) txQueue.push(static_cast<uint8_t>(byte));
        if (newline) txQueue.push('\n');
        return true;
    }
    size_t takeTx(uint8_t* output, size_t capacity)
    {
        std::lock_guard<std::mutex> lock(mutex);
        size_t count = 0;
        while (count < capacity && !txQueue.empty()) {
            output[count++] = txQueue.front();
            txQueue.pop();
        }
        return count;
    }
    void appendRx(const uint8_t* bytes, size_t count)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (size_t index = 0; index < count; ++index) {
            if (rxQueue.size() >= kCapacity) { rxQueue.pop(); ++rxDropped; }
            rxQueue.push(bytes[index]);
        }
    }
    bool echo(const std::string& message, bool newline)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!accepting || message.size() > kCapacity - (newline ? 1 : 0)) return false;
        simulated = true;
        const auto text = std::string("[simulated echo] ") + message + (newline ? "\n" : "");
        for (const auto byte : text) {
            if (rxQueue.size() >= kCapacity) { rxQueue.pop(); ++rxDropped; }
            rxQueue.push(static_cast<uint8_t>(byte));
        }
        return true;
    }
    void requestStop()
    {
        std::lock_guard<std::mutex> lock(mutex);
        accepting = false;
        stopRequested.store(true);
    }
    void fail(const std::string& detail)
    {
        std::lock_guard<std::mutex> lock(mutex);
        accepting = false;
        stopRequested.store(true);
        error = detail;
    }
    void finishWorker(bool clean)
    {
        std::lock_guard<std::mutex> lock(mutex);
        driverActive = !clean;
        workerRunning = false;
        accepting = false;
        if (!clean) error = "UART driver cleanup failed";
        changed.notify_all();
    }
    bool waitForWorker(uint32_t timeoutMs)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] { return !workerRunning; });
    }
};

// Driver callbacks run without the queue mutex. Partial writes retain their
// remaining bytes and order. Tab5 writes make nonblocking FIFO progress under
// the sole worker's uncontended driver mutex; cleanup starts after polling stops.
class UartMonitorWorker {
public:
    explicit UartMonitorWorker(std::shared_ptr<UartMonitorState> state) : _state(std::move(state)) {}
    template <typename Read, typename Write>
    bool poll(Read read, Write write)
    {
        if (_state->stopRequested.load()) return false;
        std::array<uint8_t, 127> input {};
        const int received = read(input.data(), input.size());
        if (_state->stopRequested.load()) return false;
        if (received < 0 || received > static_cast<int>(input.size())) { _state->fail("UART receive failed"); return false; }
        if (received) _state->appendRx(input.data(), static_cast<size_t>(received));
        if (_offset == _size) { _size = _state->takeTx(_output.data(), _output.size()); _offset = 0; }
        if (_size) {
            if (_state->stopRequested.load()) return false;
            const int sent = write(_output.data() + _offset, _size - _offset);
            if (sent < 0 || static_cast<size_t>(sent) > _size - _offset) { _state->fail("UART transmit failed"); return false; }
            _offset += static_cast<size_t>(sent);
        }
        return !_state->stopRequested.load();
    }
private:
    std::shared_ptr<UartMonitorState> _state;
    std::array<uint8_t, 127> _output {};
    size_t _offset = 0, _size = 0;
};
} // namespace hal
