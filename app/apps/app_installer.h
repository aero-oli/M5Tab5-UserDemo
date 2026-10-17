/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <memory>
#include <hal/hal.h>
#include "app_catalog.h"
#include "app_launcher/app_launcher.h"
#include "app_registry.h"

/**
 * @brief App 安装回调
 *
 * @param mooncake
 */
inline void on_install_apps()
{
    mooncake::GetMooncake().installApp(std::make_unique<AppLauncher>());
    auto& apps = app_registry::registry();
    app_catalog::registerDefaultApps(apps);
    apps.installAll();
}
