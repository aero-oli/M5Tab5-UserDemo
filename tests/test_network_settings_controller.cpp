#include <cassert>
#include <functional>
#include <string>
#include <vector>

#include <apps/app_settings_network/network_controller.h>

using app_settings_network::NetworkCallbacks;
using app_settings_network::NetworkController;

struct QueuedJob {
    std::string name;
    std::function<void()> work;
};

int main()
{
    std::vector<QueuedJob> jobs;
    int scans       = 0;
    int connects    = 0;
    int disconnects = 0;
    int apStarts    = 0;
    int apStops     = 0;
    std::string connectedSsid;
    bool extAntenna = false;

    hal::HalBase::WifiStatus status;
    hal::HalBase::OtaStatus ota;
    status.state = hal::HalBase::WifiConnectionState::Idle;

    NetworkCallbacks callbacks;
    callbacks.otaStatus = [&] { return ota; };
    callbacks.status    = [&] {
        status.extAntennaEnabled = extAntenna;
        return status;
    };
    callbacks.scan = [&] {
        ++scans;
        return std::vector<hal::HalBase::WifiNetwork>{{"Workshop", -44, true, 6}, {"Guest", -71, false, 1}};
    };
    callbacks.connect = [&](const std::string& ssid, const std::string& password) {
        (void)password;
        ++connects;
        connectedSsid    = ssid;
        status.state     = hal::HalBase::WifiConnectionState::Connected;
        status.ssid      = ssid;
        status.ipAddress = "192.168.1.42";
        return true;
    };
    callbacks.disconnect = [&] {
        ++disconnects;
        status = {};
    };
    callbacks.setExtAntenna = [&](bool enabled) { extAntenna = enabled; };
    callbacks.startAp       = [&] {
        ++apStarts;
        status.state     = hal::HalBase::WifiConnectionState::ApRunning;
        status.ssid      = "M5Tab5-UserDemo-WiFi";
        status.ipAddress = "192.168.4.1";
        return true;
    };
    callbacks.stopAp = [&] {
        ++apStops;
        status = {};
    };
    callbacks.startJob = [&](const std::string& name, std::function<void()> work) {
        jobs.push_back({name, std::move(work)});
        return true;
    };

    NetworkController controller(callbacks);
    auto snapshot = controller.refresh();
    assert(snapshot.detail == "Ready");

    assert(!controller.connect("", "secret"));
    assert(connects == 0);
    snapshot = controller.refresh();
    assert(snapshot.detail == "SSID is required.");

    assert(controller.scan());
    assert(jobs.size() == 1);
    assert(jobs.back().name == "Wi-Fi scan");
    assert(scans == 0);
    snapshot = controller.refresh();
    assert(snapshot.busy);
    assert(snapshot.detail == "Scanning for Wi-Fi networks...");
    jobs.back().work();
    snapshot = controller.refresh();
    assert(!snapshot.busy);
    assert(scans == 1);
    assert(snapshot.networks.size() == 2);
    assert(snapshot.detail == "Found 2 Wi-Fi networks.");

    assert(controller.connect("Workshop", "secret"));
    assert(jobs.size() == 2);
    assert(connects == 0);
    jobs.back().work();
    snapshot = controller.refresh();
    assert(connects == 1);
    assert(connectedSsid == "Workshop");
    assert(snapshot.detail == "Connected to Workshop.");
    assert(snapshot.status.state == hal::HalBase::WifiConnectionState::Connected);

    controller.setExtAntennaEnabled(true);
    assert(!extAntenna);  // The UI request must not run blocking hardware IO inline.
    assert(controller.refresh().busy);
    jobs.back().work();
    snapshot = controller.refresh();
    assert(snapshot.status.extAntennaEnabled);

    assert(controller.startAccessPoint());
    jobs.back().work();
    snapshot = controller.refresh();
    assert(apStarts == 1);
    assert(snapshot.detail == "Access point started.");

    assert(controller.stopAccessPoint());
    jobs.back().work();
    snapshot = controller.refresh();
    assert(apStops == 1);
    assert(snapshot.detail == "Access point stopped.");

    assert(controller.disconnect());
    jobs.back().work();
    snapshot = controller.refresh();
    assert(disconnects == 1);
    assert(snapshot.detail == "Disconnected.");

    ota.state         = hal::HalBase::OtaState::Downloading;
    const auto before = jobs.size();
    assert(controller.refresh().updateActive);
    assert(!controller.scan() && !controller.connect("Workshop", "secret"));
    assert(!controller.disconnect() && !controller.startAccessPoint() && !controller.stopAccessPoint());
    controller.setExtAntennaEnabled(false);
    assert(jobs.size() == before && extAntenna);

    return 0;
}
