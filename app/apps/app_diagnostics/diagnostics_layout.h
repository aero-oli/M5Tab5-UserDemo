/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_diagnostics {

inline constexpr int kDiagnosticsMetricCount = 4;

struct DiagnosticsLayout {
    app_ui::Rect content;
    app_ui::Rect description;
    int cardW  = 280;
    int cardH  = 132;
    int colGap = 24;
    int startY = 72;
    app_ui::Rect detailPanel;
    app_ui::Rect detailHint;
    app_ui::Rect detailText;
};

inline app_ui::Rect diagnosticsContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline app_ui::Rect diagnosticsMetricRect(const DiagnosticsLayout& layout, int index)
{
    return {index * (layout.cardW + layout.colGap), layout.startY, layout.cardW, layout.cardH};
}

inline DiagnosticsLayout makeDiagnosticsLayout()
{
    DiagnosticsLayout layout;
    layout.content         = diagnosticsContentRect();
    layout.description     = {0, 0, layout.content.w, 32};
    const auto firstMetric = diagnosticsMetricRect(layout, 0);
    layout.detailPanel     = {0, firstMetric.y + firstMetric.h + 32, 1192, 220};
    layout.detailHint      = {0, 0, layout.detailPanel.w - 32, 56};
    layout.detailText      = {0, 72, layout.detailPanel.w - 32, layout.detailPanel.h - 96};
    return layout;
}

}  // namespace app_diagnostics
