/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_settings {

struct AboutLayout {
    app_ui::Rect content;
    app_ui::Rect deviceCard;
    app_ui::Rect displayCard;
    int cardGap = 24;
    app_ui::Rect infoPanel;
    app_ui::Rect registryText;
    app_ui::Rect categoryText;
};

inline app_ui::Rect aboutContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline AboutLayout makeAboutLayout()
{
    AboutLayout layout;
    layout.content = aboutContentRect();
    layout.deviceCard = {0, 0, 360, 132};
    layout.displayCard = {layout.deviceCard.x + layout.deviceCard.w + layout.cardGap, 0, 360, 132};
    layout.infoPanel = {0, 156, 920, 220};
    layout.registryText = {0, 0, layout.infoPanel.w - 32, 56};
    layout.categoryText = {0, 72, layout.infoPanel.w - 32, 56};
    return layout;
}

}  // namespace app_settings
