/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace launcher_view {

struct LauncherLayout {
    app_ui::Rect screen;
    app_ui::Rect safe;
    app_ui::Rect topBar;
    app_ui::Rect categoryRail;
    app_ui::Rect tileArea;
    app_ui::Rect summaryArea;
    app_ui::Rect tileIcon;
    app_ui::Rect tileTitle;
    app_ui::Rect tileDescription;
    app_ui::Rect tileStatus;
    int columnGap = 20;
    int categoryHeadingX = 18;
    int categoryHeadingY = 16;
    int categoryButtonX = 14;
    int categoryButtonY = 62;
    int categoryButtonW = 182;
    int categoryButtonH = 52;
    int categoryButtonGap = 12;
    int tileHeadingX = 20;
    int tileHeadingY = 16;
    int tileGridX = 20;
    int tileGridY = 62;
    int tileW = 214;
    int tileH = 142;
    int tileGap = 18;
    int tileColumns = 3;
    int summaryHeadingX = 18;
    int summaryHeadingY = 16;
    int summaryRowY = 64;
    int summaryRowH = 38;
    int summaryLabelX = 18;
    int summaryLabelW = 126;
    int summaryValueW = 150;
    int summaryRightPad = 18;
};

inline LauncherLayout makeLauncherLayout()
{
    auto screen = app_ui::screenRect();
    auto safe = app_ui::safeContentRect();
    constexpr int railW = 210;
    constexpr int summaryW = 318;
    constexpr int gap = 20;
    const auto columns = app_ui::makeColumns(safe, {railW, 0, summaryW}, gap);

    LauncherLayout layout;
    layout.screen = screen;
    layout.safe = safe;
    layout.topBar = {0, 0, screen.w, 72};
    layout.categoryRail = columns.size() == 3 ? columns[0] : app_ui::Rect{safe.x, safe.y, railW, safe.h};
    layout.tileArea = columns.size() == 3 ? columns[1] : app_ui::Rect{safe.x + railW + gap, safe.y, 0, safe.h};
    layout.summaryArea = columns.size() == 3 ? columns[2] : app_ui::Rect{safe.x + railW + gap, safe.y, summaryW, safe.h};
    layout.columnGap = gap;
    layout.tileColumns = 3;
    layout.tileGap = 18;
    layout.tileGridX = 20;
    layout.tileGridY = 62;
    constexpr int tileRightMargin = 16;
    layout.tileW = (layout.tileArea.w - layout.tileGridX - tileRightMargin - (layout.tileGap * (layout.tileColumns - 1))) /
                   layout.tileColumns;
    layout.tileH = 142;
    layout.tileIcon = {14, 14, 42, 42};
    layout.tileTitle = {66, 17, layout.tileW - 82, 24};
    layout.tileDescription = {16, 70, layout.tileW - 32, 44};
    layout.tileStatus = {16, layout.tileH - 36, layout.tileW - 32, 24};
    return layout;
}

inline app_ui::Rect launcherCategoryButtonRect(const LauncherLayout& layout, int index)
{
    return {
        layout.categoryButtonX,
        layout.categoryButtonY + index * (layout.categoryButtonH + layout.categoryButtonGap),
        layout.categoryButtonW,
        layout.categoryButtonH,
    };
}

inline app_ui::Rect launcherTileRect(const LauncherLayout& layout, int index)
{
    const int col = index % layout.tileColumns;
    const int row = index / layout.tileColumns;
    return {
        layout.tileGridX + col * (layout.tileW + layout.tileGap),
        layout.tileGridY + row * (layout.tileH + layout.tileGap),
        layout.tileW,
        layout.tileH,
    };
}

inline app_ui::Rect launcherSummaryLabelRect(const LauncherLayout& layout, int index)
{
    return {
        layout.summaryLabelX,
        layout.summaryRowY + index * layout.summaryRowH,
        layout.summaryLabelW,
        24,
    };
}

inline app_ui::Rect launcherSummaryValueRect(const LauncherLayout& layout, int index)
{
    return {
        layout.summaryArea.w - layout.summaryRightPad - layout.summaryValueW,
        layout.summaryRowY + index * layout.summaryRowH,
        layout.summaryValueW,
        24,
    };
}

}  // namespace launcher_view
