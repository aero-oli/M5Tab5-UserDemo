/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <mooncake.h>

namespace app_hardware_sd {

std::unique_ptr<mooncake::AppAbility> makeSdCardApp();

}  // namespace app_hardware_sd
