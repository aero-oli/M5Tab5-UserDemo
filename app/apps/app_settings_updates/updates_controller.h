/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <string>

#include <hal/hal.h>

namespace app_settings_updates {

struct UpdatesSnapshot {
    hal::HalBase::WifiStatus wifi;
    hal::HalBase::OtaStatus ota;
    std::string detail;
    bool canInstall = false;
};

struct UpdatesCallbacks {
    std::function<hal::HalBase::WifiStatus()> wifiStatus;
    std::function<hal::HalBase::OtaStatus()> otaStatus;
    std::function<bool(const std::string&)> start;
    std::function<void()> cancel;
    std::function<void()> reboot;
};

class UpdatesController {
public:
    explicit UpdatesController(UpdatesCallbacks callbacks);

    UpdatesSnapshot refresh();
    bool start(const std::string& url);
    void cancel();
    void rebootToApply();

private:
    static bool isInstallBusy(hal::HalBase::OtaState state);
    static bool isValidUrl(const std::string& url);

    UpdatesCallbacks _callbacks;
    std::string _detail;
};

}  // namespace app_settings_updates
