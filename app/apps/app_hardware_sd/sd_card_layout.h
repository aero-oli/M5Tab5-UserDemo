/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_hardware_sd {

struct SdCardLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    app_ui::Rect scanButton;
    int sectionGap = 32;
    app_ui::Rect resultsPanel;
    app_ui::Rect resultsTitle;
    app_ui::Rect resultsText;
};

inline app_ui::Rect sdCardContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline SdCardLayout makeSdCardLayout()
{
    SdCardLayout layout;
    layout.content = sdCardContentRect();
    layout.description = {0, 0, layout.content.w, 32};
    layout.scanButton = {0, 72, 220, 60};
    layout.resultsPanel = {0, layout.scanButton.y + layout.scanButton.h + layout.sectionGap, 900, 392};
    layout.resultsTitle = {0, 0, layout.resultsPanel.w - 32, 30};
    layout.resultsText = {0, 48, layout.resultsPanel.w - 32, layout.resultsPanel.h - 76};
    return layout;
}

}  // namespace app_hardware_sd
