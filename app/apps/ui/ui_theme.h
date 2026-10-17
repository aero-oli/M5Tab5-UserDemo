/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>

namespace app_ui::theme {

constexpr uint32_t bg = 0x111315;
constexpr uint32_t panel = 0x1A1D21;
constexpr uint32_t panelAlt = 0x242A30;
constexpr uint32_t border = 0x3B424A;
constexpr uint32_t text = 0xF4F7FA;
constexpr uint32_t muted = 0xAEB7C2;
constexpr uint32_t blue = 0x2F80ED;
constexpr uint32_t green = 0x28A36A;
constexpr uint32_t amber = 0xC98B2E;
constexpr uint32_t red = 0xD64545;
constexpr uint32_t surfaceRaised = 0x20242A;
constexpr uint32_t focus = 0x7FB3FF;

inline lv_color_t color(uint32_t hex)
{
    return lv_color_hex(hex);
}

}  // namespace app_ui::theme
