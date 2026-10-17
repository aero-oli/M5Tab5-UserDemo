/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_hardware_power_monitor {

inline constexpr int kPowerMonitorMetricCount = 4;

struct PowerMonitorLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    int cardW = 280;
    int cardH = 132;
    int colGap = 24;
    int startY = 72;
    app_ui::Rect readoutPanel;
    app_ui::Rect readoutTitle;
    app_ui::Rect readoutDetail;
};

inline app_ui::Rect powerMonitorContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline app_ui::Rect powerMonitorMetricRect(const PowerMonitorLayout& layout, int index)
{
    return {index * (layout.cardW + layout.colGap), layout.startY, layout.cardW, layout.cardH};
}

inline PowerMonitorLayout makePowerMonitorLayout()
{
    PowerMonitorLayout layout;
    layout.content = powerMonitorContentRect();
    layout.description = {0, 0, layout.content.w, 32};
    const auto firstMetric = powerMonitorMetricRect(layout, 0);
    layout.readoutPanel = {0, firstMetric.y + firstMetric.h + 32, 1192, 180};
    layout.readoutTitle = {0, 0, layout.readoutPanel.w - 32, 30};
    layout.readoutDetail = {0, 48, layout.readoutPanel.w - 32, layout.readoutPanel.h - 76};
    return layout;
}

}  // namespace app_hardware_power_monitor
