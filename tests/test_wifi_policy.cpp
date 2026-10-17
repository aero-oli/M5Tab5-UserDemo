#include <cassert>
#include <string>
#include <hal/network_state.h>
#include <hal/wifi_credentials.h>

int main()
{
    hal::WifiCredentialBytes bytes;
    const std::string fullSsid(32, 's');
    assert(hal::encodeWifiCredentials(fullSsid, "password", bytes));
    for (auto value : bytes.ssid) assert(value == 's');  // All 32 bytes survive, without forced NUL.
    assert(hal::encodeWifiCredentials("Open", "", bytes));
    for (auto value : bytes.password) assert(value == 0);
    assert(hal::encodeWifiCredentials("PSK", std::string(64, 'a'), bytes));
    for (auto value : bytes.password) assert(value == 'a');
    assert(!hal::encodeWifiCredentials("", "password", bytes));
    assert(!hal::encodeWifiCredentials(std::string(33, 's'), "password", bytes));
    assert(!hal::encodeWifiCredentials("SSID", "short", bytes));
    assert(!hal::encodeWifiCredentials("SSID", std::string(64, 'z'), bytes));
    assert(!hal::encodeWifiCredentials("SSID", std::string(65, 'a'), bytes));
    assert(!hal::encodeWifiCredentials(std::string("bad\0SSID", 8), "password", bytes));
    assert(!hal::encodeWifiCredentials("SSID", std::string("password\0tail", 13), bytes));

    hal::NetworkState state;
    const auto first = state.beginStation("First", 100);
    assert(state.stationConnected(first, "First", "192.168.1.2", -42));
    auto scan = state.beginScan();
    state.finishScan(scan, true);
    assert(state.snapshot().wifi.state == hal::HalBase::WifiConnectionState::Connected);
    assert(state.snapshot().wifi.ipAddress == "192.168.1.2");
    scan = state.beginScan();
    state.stationDisconnected(first, "First", "Link lost during scan");
    state.finishScan(scan, true);
    assert(state.snapshot().wifi.state == hal::HalBase::WifiConnectionState::Failed);
    assert(state.snapshot().wifi.ipAddress.empty());
    state.stopStationIntent();
    assert(!state.stationConnected(first, "First", "192.168.1.2", -30));
    const auto second = state.beginStation("Second", 500);
    assert(!state.stationDisconnected(first, "First", "Late old disconnect"));
    assert(!state.stationConnected(second, "First", "192.168.1.2", -30));
    assert(state.snapshot().wifi.state == hal::HalBase::WifiConnectionState::Connecting);
    assert(!state.expireStation(20499, 20000));
    assert(state.expireStation(20500, 20000));
    assert(!state.stationConnected(second, "Second", "192.168.1.3", -35));
    const auto third = state.beginStation("Second", 21000);
    assert(!state.stationDisconnected(second, "Second", "Delayed same-SSID old attempt"));
    assert(state.stationConnected(third, "Second", "192.168.1.3", -35));
    assert(!state.expireStation(99999, 20000));
    assert(state.reserveOta());
    assert(!state.allowsCommands());
    assert(state.enterOtaCommit());
    state.cancelOta();
    assert(!state.otaCancelled());  // Commit is atomic and no longer cancellable.
    state.quiesce();
    assert(!state.allowsCommands() && state.snapshot().otaActive);
    return 0;
}
