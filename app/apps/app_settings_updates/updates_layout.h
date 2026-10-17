/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_settings_updates {

struct UpdatesLayout {
    app_ui::Rect content;
    app_ui::Rect firmwareCard;
    app_ui::Rect stateCard;
    app_ui::Rect otaPanel;
    app_ui::Rect urlInput;
    app_ui::Rect installButton;
    app_ui::Rect cancelButton;
    app_ui::Rect rebootButton;
    app_ui::Rect progressBar;
    app_ui::Rect detailText;
};

inline UpdatesLayout makeUpdatesLayout()
{
    return {
        {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h},
        {0, 0, 360, 132},
        {384, 0, 360, 132},
        {0, 156, 920, 188},
        {0, 86, 872, 54},
        {0, 372, 210, 60},
        {230, 372, 220, 60},
        {470, 372, 140, 60},
        {0, 468, 920, 28},
        {0, 520, 920, 80},
    };
}

}  // namespace app_settings_updates
