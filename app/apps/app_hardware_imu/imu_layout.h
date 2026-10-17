/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_hardware_imu {

inline constexpr int kImuMetricCount = 6;

struct ImuLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    int cols   = 3;
    int cardW  = 280;
    int cardH  = 132;
    int colGap = 24;
    int rowGap = 24;
    int startY = 72;
    app_ui::Rect detailText;
};

inline app_ui::Rect imuContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline app_ui::Rect imuMetricRect(const ImuLayout& layout, int index)
{
    const int col = index % layout.cols;
    const int row = index / layout.cols;
    return {col * (layout.cardW + layout.colGap), layout.startY + row * (layout.cardH + layout.rowGap), layout.cardW,
            layout.cardH};
}

inline ImuLayout makeImuLayout()
{
    ImuLayout layout;
    layout.content        = imuContentRect();
    layout.description    = {0, 0, layout.content.w, 32};
    const auto lastMetric = imuMetricRect(layout, kImuMetricCount - 1);
    layout.detailText     = {0, lastMetric.y + lastMetric.h + 32, 900, 32};
    return layout;
}

}  // namespace app_hardware_imu
