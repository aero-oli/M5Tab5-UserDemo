/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_hardware_switches {

inline constexpr int kSwitchControlCount = 5;

struct SwitchesLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    int cols       = 2;
    int cardW      = 0;
    int cardH      = 88;
    int gridGap    = 24;
    int startY     = 72;
    int sectionGap = 24;
    app_ui::Rect switchControl;
    app_ui::Rect cardTitle;
    app_ui::Rect cardDetail;
    app_ui::Rect interfaceStatus;
    app_ui::Rect statusTitle;
    app_ui::Rect statusDetail;
    int bottomPadding       = 24;
    int scrollContentHeight = 0;
};

inline app_ui::Rect switchesContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline app_ui::Rect switchCardRect(const SwitchesLayout& layout, int index)
{
    const int col = index % layout.cols;
    const int row = index / layout.cols;
    return {
        col * (layout.cardW + layout.gridGap),
        layout.startY + row * (layout.cardH + layout.gridGap),
        layout.cardW,
        layout.cardH,
    };
}

inline SwitchesLayout makeSwitchesLayout()
{
    SwitchesLayout layout;
    layout.content       = switchesContentRect();
    layout.cardW         = (layout.content.w - layout.gridGap) / layout.cols;
    layout.description   = {0, 0, layout.content.w, 32};
    layout.switchControl = {layout.cardW - 112, 9, 72, 38};
    layout.cardTitle     = {0, 0, layout.switchControl.x - 24, 26};
    layout.cardDetail    = {0, 34, layout.switchControl.x - 24, 22};

    const auto lastCard        = switchCardRect(layout, kSwitchControlCount - 1);
    layout.interfaceStatus     = {0, lastCard.y + lastCard.h + layout.sectionGap, layout.content.w, 112};
    layout.statusTitle         = {0, 0, layout.interfaceStatus.w - 32, 28};
    layout.statusDetail        = {0, 46, layout.interfaceStatus.w - 32, 28};
    layout.scrollContentHeight = layout.interfaceStatus.y + layout.interfaceStatus.h + layout.bottomPadding;
    return layout;
}

}  // namespace app_hardware_switches
