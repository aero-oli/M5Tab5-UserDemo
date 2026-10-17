/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_media_audio {

struct AudioToolLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    app_ui::Rect stateCard;
    int sectionGap = 32;
    int actionGap  = 24;
    app_ui::Rect startButton;
    app_ui::Rect stopButton;
    bool hasStopButton = false;
    app_ui::Rect statusPanel;
    app_ui::Rect statusTitle;
    app_ui::Rect statusDetail;
};

inline app_ui::Rect audioToolContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline AudioToolLayout makeAudioToolLayout(bool hasStopButton)
{
    AudioToolLayout layout;
    layout.content       = audioToolContentRect();
    layout.description   = {0, 0, layout.content.w, 32};
    layout.stateCard     = {0, 72, 320, 132};
    layout.startButton   = {0, layout.stateCard.y + layout.stateCard.h + layout.sectionGap, 240, 64};
    layout.hasStopButton = hasStopButton;
    if (hasStopButton) {
        layout.stopButton = {layout.startButton.x + layout.startButton.w + layout.actionGap, layout.startButton.y, 160,
                             layout.startButton.h};
    }
    layout.statusPanel  = {0, layout.startButton.y + layout.startButton.h + layout.sectionGap, 760, 156};
    layout.statusTitle  = {0, 0, layout.statusPanel.w - 32, 30};
    layout.statusDetail = {0, 48, layout.statusPanel.w - 32, layout.statusPanel.h - 76};
    return layout;
}

}  // namespace app_media_audio
