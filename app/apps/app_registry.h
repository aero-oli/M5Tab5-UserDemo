/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <mooncake.h>

namespace app_registry {

enum class AppKey {
    Rtc,
    DisplaySound,
    PowerMonitor,
    Imu,
    Switches,
    SettingsPower,
    Camera,
    DualMic,
    Headphone,
    SdCard,
    I2cScan,
    Gpio,
    Music,
    ComMonitor,
    Network,
    Updates,
    About,
    Diagnostics,
    Pps,
};

enum class AppCategory {
    Home,
    Settings,
    Hardware,
    Media,
    Modules,
};

enum class StatusLevel {
    Neutral,
    Ok,
    Warning,
    Error,
};

struct AppStatus {
    std::string text;
    StatusLevel level = StatusLevel::Neutral;
};

using AppFactory     = std::function<std::unique_ptr<mooncake::AppAbility>()>;
using StatusProvider = std::function<AppStatus()>;

struct RegisteredApp {
    AppKey key;
    AppCategory category;
    std::string label;
    std::string description;
    std::string defaultStatus;
    std::string icon;
    int order             = 0;
    int summaryPriority   = 0;
    bool dangerous        = false;
    bool dashboardVisible = true;
    AppFactory factory;
    StatusProvider statusProvider;
    int appId    = -1;
    bool visible = true;
};

class AppRegistry {
public:
    void add(RegisteredApp app);
    void installAll();
    bool open(AppKey key);

    const std::vector<RegisteredApp>& all() const;
    std::vector<const RegisteredApp*> byCategory(AppCategory category) const;
    std::vector<const RegisteredApp*> dashboardApps() const;
    RegisteredApp* find(AppKey key);
    const RegisteredApp* find(AppKey key) const;
    AppStatus status(AppKey key) const;

private:
    std::vector<RegisteredApp> _apps;
};

AppRegistry& registry();

const char* categoryName(AppCategory category);
const std::vector<AppCategory>& categories();

}  // namespace app_registry
