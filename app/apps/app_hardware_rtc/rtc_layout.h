/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_hardware_rtc {

struct RtcLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    app_ui::Rect timeCard;
    app_ui::Rect dateCard;
    int cardGap = 24;
    app_ui::Rect sourcePanel;
    app_ui::Rect sourceTitle;
    app_ui::Rect sourceDetail;
};

inline app_ui::Rect rtcContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline RtcLayout makeRtcLayout()
{
    RtcLayout layout;
    layout.content = rtcContentRect();
    layout.description = {0, 0, layout.content.w, 32};
    layout.timeCard = {0, 72, 360, 132};
    layout.dateCard = {layout.timeCard.x + layout.timeCard.w + layout.cardGap, 72, 360, 132};
    layout.sourcePanel = {0, 236, 744, 160};
    layout.sourceTitle = {0, 0, layout.sourcePanel.w - 32, 30};
    layout.sourceDetail = {0, 48, layout.sourcePanel.w - 32, layout.sourcePanel.h - 76};
    return layout;
}

}  // namespace app_hardware_rtc
