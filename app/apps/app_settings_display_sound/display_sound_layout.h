/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_settings_display_sound {

inline constexpr int kDisplaySoundActionCount = 4;

struct DisplaySoundLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    app_ui::Rect brightnessCard;
    app_ui::Rect volumeCard;
    int columnGap = 32;
    int actionY = 252;
    int actionH = 58;
    int actionGap = 16;
    app_ui::Rect infoPanel;
    app_ui::Rect infoText;
};

inline app_ui::Rect displaySoundContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline app_ui::Rect displaySoundActionRect(const DisplaySoundLayout& layout, int index)
{
    switch (index) {
        case 0:
            return {layout.brightnessCard.x, layout.actionY, 184, layout.actionH};
        case 1:
            return {layout.brightnessCard.x + 200, layout.actionY, 184, layout.actionH};
        case 2:
            return {layout.volumeCard.x, layout.actionY, 160, layout.actionH};
        case 3:
            return {layout.volumeCard.x + 176, layout.actionY, 160, layout.actionH};
        default:
            return {};
    }
}

inline DisplaySoundLayout makeDisplaySoundLayout()
{
    DisplaySoundLayout layout;
    layout.content = displaySoundContentRect();
    layout.description = {0, 0, layout.content.w, 32};
    layout.brightnessCard = {0, 72, 384, 150};
    layout.volumeCard = {layout.brightnessCard.x + layout.brightnessCard.w + layout.columnGap, 72, 360, 150};
    layout.infoPanel = {0, 348, 800, 126};
    layout.infoText = {0, 0, layout.infoPanel.w - 32, 32};
    return layout;
}

}  // namespace app_settings_display_sound
