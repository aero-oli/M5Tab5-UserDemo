/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */
#include "updates_controller.h"
#include <hal/ota_policy.h>
#include <utility>

using namespace app_settings_updates;
using State = hal::HalBase::OtaState;

UpdatesController::UpdatesController(UpdatesCallbacks callbacks) : _callbacks(std::move(callbacks)) {}

UpdatesSnapshot UpdatesController::refresh()
{
    UpdatesSnapshot snapshot;
    if (_callbacks.wifiStatus) snapshot.wifi = _callbacks.wifiStatus();
    if (_callbacks.otaStatus) snapshot.ota = _callbacks.otaStatus();
    const auto& ota = snapshot.ota;
    snapshot.busy = isInstallBusy(ota.state);
    snapshot.canInstall = snapshot.wifi.state == hal::HalBase::WifiConnectionState::Connected &&
                          !snapshot.busy && !ota.rebootRequired;
    snapshot.canCancel = snapshot.busy && ota.canCancel && ota.state != State::Cancelling;
    snapshot.canReboot = ota.state == State::Complete && ota.rebootRequired;
    switch (ota.state) {
        case State::Checking: snapshot.stateText = "Checking"; break;
        case State::Downloading: snapshot.stateText = "Downloading"; break;
        case State::Installing: snapshot.stateText = "Finishing"; break;
        case State::Complete: snapshot.stateText = "Ready to reboot"; break;
        case State::Failed: snapshot.stateText = "Failed"; break;
        case State::Cancelling: snapshot.stateText = "Cancelling"; break;
        case State::Cancelled: snapshot.stateText = "Cancelled"; break;
        default: snapshot.stateText = "Idle"; break;
    }
    if (ota.simulated) snapshot.stateText = "Sim: " + snapshot.stateText;
    if (ota.progressKnown) {
        snapshot.progressText = std::to_string(ota.progress) + "% | " + std::to_string(ota.bytesReceived) +
                                " / " + std::to_string(ota.totalBytes) + " bytes";
    } else if (snapshot.busy || ota.bytesReceived) {
        snapshot.progressText = std::to_string(ota.bytesReceived) + " bytes received | total size unknown";
    }
    if (!_inputError.empty()) snapshot.detail = _inputError;
    else if (!ota.lastError.empty()) snapshot.detail = ota.lastError;
    else if (snapshot.canReboot) snapshot.detail = ota.simulated ? "Simulated update complete. No firmware was written." :
        "Firmware verified and selected. Reboot to apply it.";
    else if (ota.state == State::Cancelling) snapshot.detail = "Stopping transfer and releasing update resources...";
    else if (ota.state == State::Cancelled) snapshot.detail = "Update cancelled. The boot image was not changed.";
    else if (snapshot.busy) snapshot.detail = ota.canCancel ? "Update in progress. Back leaves the transfer running." :
        "Finishing the verified image. Cancellation is no longer available.";
    else snapshot.detail = snapshot.canInstall ? "Wi-Fi connected. Choose an HTTPS firmware URL." : "Connect Wi-Fi before OTA";
    return snapshot;
}

bool UpdatesController::prepareInstall(const std::string& url)
{
    const auto snapshot = refresh();
    if (snapshot.wifi.state != hal::HalBase::WifiConnectionState::Connected)
        _inputError = "Connect Wi-Fi before OTA";
    else if (snapshot.busy) _inputError = "Update already in progress.";
    else if (snapshot.ota.rebootRequired) _inputError = "Reboot to apply the verified update before installing another.";
    else if (!hal::validOtaUrl(url)) _inputError = "Use an HTTPS URL with a host and no embedded credentials.";
    else { _inputError.clear(); return true; }
    return false;
}

bool UpdatesController::start(const std::string& url)
{
    if (!prepareInstall(url)) return false;
    if (!_callbacks.start || !_callbacks.start(url)) {
        _inputError = "Could not start OTA. Check the connection and retry.";
        return false;
    }
    _inputError.clear();
    return true;
}

void UpdatesController::cancel()
{
    if (refresh().canCancel && _callbacks.cancel) { _inputError.clear(); _callbacks.cancel(); }
}

void UpdatesController::rebootToApply()
{
    if (refresh().canReboot && _callbacks.reboot) _callbacks.reboot();
}

bool UpdatesController::isInstallBusy(State state)
{
    return state == State::Checking || state == State::Downloading || state == State::Installing || state == State::Cancelling;
}
