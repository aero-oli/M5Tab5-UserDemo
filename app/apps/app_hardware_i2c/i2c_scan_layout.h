/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_hardware_i2c {

inline constexpr int kI2cScanActionCount = 2;

struct I2cScanLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    int actionY    = 72;
    int actionW    = 220;
    int actionH    = 60;
    int actionGap  = 24;
    int sectionGap = 32;
    app_ui::Rect resultsPanel;
    app_ui::Rect resultsTitle;
    app_ui::Rect resultsText;
};

inline app_ui::Rect i2cScanContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline app_ui::Rect i2cScanActionRect(const I2cScanLayout& layout, int index)
{
    return {index * (layout.actionW + layout.actionGap), layout.actionY, layout.actionW, layout.actionH};
}

inline I2cScanLayout makeI2cScanLayout()
{
    I2cScanLayout layout;
    layout.content         = i2cScanContentRect();
    layout.description     = {0, 0, layout.content.w, 32};
    const auto firstAction = i2cScanActionRect(layout, 0);
    layout.resultsPanel    = {0, firstAction.y + firstAction.h + layout.sectionGap, 900, 392};
    layout.resultsTitle    = {0, 0, layout.resultsPanel.w - 32, 30};
    layout.resultsText     = {0, 48, layout.resultsPanel.w - 32, layout.resultsPanel.h - 76};
    return layout;
}

}  // namespace app_hardware_i2c
