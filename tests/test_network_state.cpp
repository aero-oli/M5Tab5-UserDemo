#include <atomic>
#include <cassert>
#include <string>
#include <thread>

#include <hal/network_state.h>

int main()
{
    hal::NetworkState state;
    std::atomic<bool> finished {false};
    std::atomic<int> snapshots {0};
    state.update([](auto& value) {
        value.wifi.ssid = std::string(128, 'a');
        value.wifi.ipAddress = value.wifi.ssid;
        value.wifi.lastError = value.wifi.ssid;
        value.ota.lastError = value.wifi.ssid;
    });
    std::thread reader([&] {
        do {
            const auto snapshot = state.snapshot();
            assert(snapshot.wifi.ssid == snapshot.wifi.ipAddress);
            assert(snapshot.wifi.ssid == snapshot.wifi.lastError);
            assert(snapshot.wifi.ssid == snapshot.ota.lastError);
            assert(snapshot.ota.progress == (snapshot.otaActive ? 20 : 0));
            ++snapshots;
        } while (!finished.load());
    });
    for (int i = 0; i < 10000; ++i) {
        state.update([&](auto& value) {
            const std::string text(128 + i % 16, static_cast<char>('a' + i % 26));
            value.wifi.ssid = text;
            value.wifi.ipAddress = text;
            value.wifi.lastError = text;
            value.ota.lastError = text;
            value.otaActive = i % 2 != 0;
            value.ota.progress = value.otaActive ? 20 : 0;
        });
    }
    finished.store(true);
    reader.join();
    assert(snapshots > 0);

    // Mutating a returned copy must never alter published state.
    auto copy = state.snapshot();
    copy.wifi.ssid = "local copy";
    assert(state.snapshot().wifi.ssid != copy.wifi.ssid);

    std::atomic<bool> commandBusy {false};
    {
        hal::NetworkCommandLease first(commandBusy);
        assert(first);
        std::thread contender([&] {
            hal::NetworkCommandLease second(commandBusy);
            assert(!second);
            // Rejected lease destruction must not release another owner's lease.
        });
        contender.join();
        assert(commandBusy.load());
        // State publication and snapshots remain available during a command.
        state.update([](auto& value) { value.wifi.ssid = "event during command"; });
        assert(state.snapshot().wifi.ssid == "event during command");
    }
    assert(!commandBusy.load());
    hal::NetworkCommandLease next(commandBusy);
    assert(next);
    return 0;
}
