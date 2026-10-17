/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <mooncake.h>

namespace app_hardware_power_monitor {

std::unique_ptr<mooncake::AppAbility> makePowerMonitorApp();

}  // namespace app_hardware_power_monitor
