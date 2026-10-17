/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_registry.h"
#include <algorithm>

using namespace app_registry;

void AppRegistry::add(RegisteredApp app)
{
    auto it = std::find_if(_apps.begin(), _apps.end(),
                           [&](const RegisteredApp& existing) { return existing.key == app.key; });
    if (it != _apps.end()) {
        *it = std::move(app);
    } else {
        _apps.push_back(std::move(app));
    }

    std::stable_sort(_apps.begin(), _apps.end(), [](const RegisteredApp& a, const RegisteredApp& b) {
        if (a.category != b.category) {
            return static_cast<int>(a.category) < static_cast<int>(b.category);
        }
        return a.order < b.order;
    });
}

void AppRegistry::installAll()
{
    for (auto& app : _apps) {
        if (app.appId >= 0 || !app.factory) {
            continue;
        }
        app.appId = mooncake::GetMooncake().installApp(app.factory());
    }
}

bool AppRegistry::open(AppKey key)
{
    auto* app = find(key);
    if (app == nullptr || app->appId < 0) {
        return false;
    }
    return mooncake::GetMooncake().openApp(app->appId);
}

const std::vector<RegisteredApp>& AppRegistry::all() const
{
    return _apps;
}

std::vector<const RegisteredApp*> AppRegistry::byCategory(AppCategory category) const
{
    std::vector<const RegisteredApp*> out;
    for (const auto& app : _apps) {
        if (app.visible && app.category == category) {
            out.push_back(&app);
        }
    }
    return out;
}

std::vector<const RegisteredApp*> AppRegistry::dashboardApps() const
{
    std::vector<const RegisteredApp*> out;
    for (const auto& app : _apps) {
        if (app.visible && app.dashboardVisible) {
            out.push_back(&app);
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const RegisteredApp* a, const RegisteredApp* b) {
        return a->summaryPriority > b->summaryPriority;
    });
    return out;
}

RegisteredApp* AppRegistry::find(AppKey key)
{
    auto it = std::find_if(_apps.begin(), _apps.end(), [&](const RegisteredApp& app) { return app.key == key; });
    return it == _apps.end() ? nullptr : &(*it);
}

const RegisteredApp* AppRegistry::find(AppKey key) const
{
    auto it = std::find_if(_apps.begin(), _apps.end(), [&](const RegisteredApp& app) { return app.key == key; });
    return it == _apps.end() ? nullptr : &(*it);
}

AppStatus AppRegistry::status(AppKey key) const
{
    auto* app = find(key);
    if (app == nullptr) {
        return {};
    }
    if (app->statusProvider) {
        return app->statusProvider();
    }
    return {app->defaultStatus, StatusLevel::Neutral};
}

AppRegistry& app_registry::registry()
{
    static AppRegistry instance;
    return instance;
}

const char* app_registry::categoryName(AppCategory category)
{
    switch (category) {
        case AppCategory::Home:
            return "Home";
        case AppCategory::Settings:
            return "Settings";
        case AppCategory::Hardware:
            return "Hardware";
        case AppCategory::Media:
            return "Media";
        case AppCategory::Modules:
            return "Modules";
        default:
            return "Apps";
    }
}

const std::vector<AppCategory>& app_registry::categories()
{
    static const std::vector<AppCategory> cats = {
        AppCategory::Home, AppCategory::Settings, AppCategory::Hardware, AppCategory::Media, AppCategory::Modules,
    };
    return cats;
}
