/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <mooncake.h>

namespace app_media_audio {

std::unique_ptr<mooncake::AppAbility> makeDualMicApp();
std::unique_ptr<mooncake::AppAbility> makeHeadphoneApp();
std::unique_ptr<mooncake::AppAbility> makeMusicApp();

}  // namespace app_media_audio
