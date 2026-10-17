/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_hardware_com_monitor {

struct ComMonitorLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    app_ui::Rect logPanel;
    app_ui::Rect logTitle;
    app_ui::Rect logText;
    int sectionGap = 28;
    app_ui::Rect sendButton;
    app_ui::Rect statusText;
};

inline app_ui::Rect comMonitorContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline ComMonitorLayout makeComMonitorLayout()
{
    ComMonitorLayout layout;
    layout.content = comMonitorContentRect();
    layout.description = {0, 0, layout.content.w, 32};
    layout.logPanel = {0, 72, 980, 420};
    layout.logTitle = {0, 0, layout.logPanel.w - 32, 30};
    layout.logText = {0, 48, layout.logPanel.w - 32, layout.logPanel.h - 76};
    layout.sendButton = {0, layout.logPanel.y + layout.logPanel.h + layout.sectionGap, 260, 58};
    layout.statusText = {layout.sendButton.x + layout.sendButton.w + 24, layout.sendButton.y + 12, 800, 32};
    return layout;
}

}  // namespace app_hardware_com_monitor
