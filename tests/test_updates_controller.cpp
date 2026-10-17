#include <cassert>
#include <string>

#include <apps/app_settings_updates/updates_controller.h>

using app_settings_updates::UpdatesCallbacks;
using app_settings_updates::UpdatesController;

int main()
{
    int starts = 0;
    int cancels = 0;
    int reboots = 0;
    std::string startedUrl;

    hal::HalBase::WifiStatus wifi;
    wifi.state = hal::HalBase::WifiConnectionState::Idle;
    hal::HalBase::OtaStatus ota;

    UpdatesCallbacks callbacks;
    callbacks.wifiStatus = [&] { return wifi; };
    callbacks.otaStatus = [&] { return ota; };
    callbacks.start = [&](const std::string& url) {
        ++starts;
        startedUrl = url;
        ota.state = hal::HalBase::OtaState::Downloading;
        ota.progress = 4;
        return true;
    };
    callbacks.cancel = [&] { ++cancels; };
    callbacks.reboot = [&] { ++reboots; };

    UpdatesController controller(callbacks);
    auto snapshot = controller.refresh();
    assert(!snapshot.canInstall);
    assert(snapshot.detail == "Connect Wi-Fi before OTA");

    assert(!controller.start(""));
    assert(starts == 0);
    snapshot = controller.refresh();
    assert(snapshot.detail == "Connect Wi-Fi before OTA");

    wifi.state = hal::HalBase::WifiConnectionState::Connected;
    assert(!controller.start("ftp://example.com/firmware.bin"));
    assert(starts == 0);
    snapshot = controller.refresh();
    assert(snapshot.detail == "URL must start with http:// or https://");

    assert(controller.start("https://example.com/m5stack_tab5.bin"));
    assert(starts == 1);
    assert(startedUrl == "https://example.com/m5stack_tab5.bin");
    snapshot = controller.refresh();
    assert(snapshot.ota.state == hal::HalBase::OtaState::Downloading);
    assert(snapshot.ota.progress == 4);

    controller.cancel();
    controller.rebootToApply();
    assert(cancels == 1);
    assert(reboots == 1);

    ota.state = hal::HalBase::OtaState::Complete;
    ota.progress = 100;
    ota.rebootRequired = true;
    snapshot = controller.refresh();
    assert(snapshot.detail == "Update installed. Reboot to apply it.");

    return 0;
}
