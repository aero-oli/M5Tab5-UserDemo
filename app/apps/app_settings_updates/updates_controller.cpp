/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "updates_controller.h"

#include <utility>

using namespace app_settings_updates;

UpdatesController::UpdatesController(UpdatesCallbacks callbacks) : _callbacks(std::move(callbacks)) {}

UpdatesSnapshot UpdatesController::refresh()
{
    UpdatesSnapshot snapshot;
    if (_callbacks.wifiStatus) {
        snapshot.wifi = _callbacks.wifiStatus();
    }
    if (_callbacks.otaStatus) {
        snapshot.ota = _callbacks.otaStatus();
    }

    const bool wifiConnected = snapshot.wifi.state == hal::HalBase::WifiConnectionState::Connected;
    snapshot.canInstall = wifiConnected && !isInstallBusy(snapshot.ota.state);

    if (!snapshot.ota.lastError.empty()) {
        snapshot.detail = snapshot.ota.lastError;
    } else if (snapshot.ota.rebootRequired) {
        snapshot.detail = "Update installed. Reboot to apply it.";
    } else if (!_detail.empty()) {
        snapshot.detail = _detail;
    } else if (wifiConnected) {
        snapshot.detail = "Wi-Fi connected";
    } else {
        snapshot.detail = "Connect Wi-Fi before OTA";
    }
    return snapshot;
}

bool UpdatesController::start(const std::string& url)
{
    auto snapshot = refresh();
    if (snapshot.wifi.state != hal::HalBase::WifiConnectionState::Connected) {
        _detail = "Connect Wi-Fi before OTA";
        return false;
    }
    if (!isValidUrl(url)) {
        _detail = "URL must start with http:// or https://";
        return false;
    }
    if (isInstallBusy(snapshot.ota.state)) {
        _detail = "Update already in progress.";
        return false;
    }
    if (!_callbacks.start || !_callbacks.start(url)) {
        _detail = "Could not start OTA.";
        return false;
    }
    _detail = "Starting update...";
    return true;
}

void UpdatesController::cancel()
{
    if (_callbacks.cancel) {
        _callbacks.cancel();
    }
}

void UpdatesController::rebootToApply()
{
    if (_callbacks.reboot) {
        _callbacks.reboot();
    }
}

bool UpdatesController::isInstallBusy(hal::HalBase::OtaState state)
{
    using State = hal::HalBase::OtaState;
    return state == State::Checking || state == State::Downloading || state == State::Installing;
}

bool UpdatesController::isValidUrl(const std::string& url)
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}
