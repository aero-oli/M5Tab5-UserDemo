#include <cassert>
#include <string>
#include <apps/app_settings_updates/updates_controller.h>

using app_settings_updates::UpdatesCallbacks;
using app_settings_updates::UpdatesController;
using State = hal::HalBase::OtaState;

int main()
{
    int starts = 0, cancels = 0, reboots = 0;
    bool acceptStart = true;
    std::string startedUrl;
    hal::HalBase::WifiStatus wifi;
    hal::HalBase::OtaStatus ota;
    UpdatesCallbacks callbacks;
    callbacks.wifiStatus = [&] { return wifi; };
    callbacks.otaStatus  = [&] { return ota; };
    callbacks.start      = [&](const std::string& url) {
        ++starts;
        startedUrl = url;
        if (acceptStart) {
            ota.state     = State::Checking;
            ota.canCancel = true;
        }
        return acceptStart;
    };
    callbacks.cancel = [&] {
        ++cancels;
        ota.state     = State::Cancelling;
        ota.canCancel = false;
    };
    callbacks.reboot = [&] { ++reboots; };
    UpdatesController controller(callbacks);
    assert(!controller.refresh().canInstall);
    assert(!controller.prepareInstall("https://example.com/fw.bin") && starts == 0);
    controller.cancel();
    controller.rebootToApply();
    assert(cancels == 0 && reboots == 0);
    wifi.state = hal::HalBase::WifiConnectionState::Connected;
    for (const auto* invalid : {"", "http://example.com/fw.bin", "https://", "https://user:secret@example.com/fw.bin",
                                "https://example.com/a b"}) {
        assert(!controller.start(invalid) && starts == 0);
        assert(controller.refresh().detail.find("HTTPS") != std::string::npos);
    }
    const std::string url = "https://example.com/m5stack_tab5.bin";
    assert(controller.prepareInstall(url) && starts == 0);  // Dialog preparation never starts a transfer.
    assert(controller.start(url) && starts == 1 && startedUrl == url);
    auto snapshot = controller.refresh();
    assert(snapshot.busy && !snapshot.canInstall && snapshot.canCancel && !snapshot.canReboot);
    assert(!controller.start(url) && starts == 1);
    ota.state         = State::Downloading;
    ota.bytesReceived = 4096;
    snapshot          = controller.refresh();
    assert(snapshot.progressText.find("4096 bytes") != std::string::npos);
    assert(snapshot.progressText.find('%') == std::string::npos);  // Unknown length is never a fabricated percent.
    controller.cancel();
    controller.cancel();
    assert(cancels == 1 && !controller.refresh().canCancel);
    ota.state = State::Cancelled;
    assert(controller.refresh().canInstall && !controller.refresh().canReboot);
    assert(controller.start(url) && starts == 2);
    ota.state     = State::Installing;
    ota.canCancel = false;
    controller.cancel();
    controller.rebootToApply();
    assert(cancels == 1 && reboots == 0);
    ota.state         = State::Complete;
    ota.progressKnown = true;
    ota.progress      = 100;
    ota.totalBytes = ota.bytesReceived = 8192;
    ota.rebootRequired                 = true;
    snapshot                           = controller.refresh();
    assert(snapshot.canReboot && !snapshot.canInstall && !snapshot.canCancel && !snapshot.busy);
    assert(snapshot.progressText == "100% | 8192 / 8192 bytes");
    assert(!controller.start(url) && starts == 2);
    controller.rebootToApply();
    assert(reboots == 1);
    ota           = {};
    ota.simulated = true;
    ota.state     = State::Failed;
    ota.lastError = "Image rejected";
    assert(controller.refresh().stateText == "Sim: Failed");
    assert(controller.prepareInstall(url));
    assert(controller.refresh().detail == "Image rejected");
    acceptStart = false;
    assert(!controller.start(url));
    assert(controller.refresh().detail.find("Could not start") != std::string::npos);
    ota.lastError.clear();
    acceptStart = true;
    assert(controller.start(url));
    assert(controller.refresh().detail.find("Could not start") == std::string::npos);
}
