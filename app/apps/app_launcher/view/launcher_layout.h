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
    app_ui::Rect headerTitle;
    app_ui::Rect headerStatus;
    app_ui::Rect headerTime;
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
    int tileW = 308;
    int tileH = 184;
    int tileGap = 18;
    int tileColumns = 3;
};

inline LauncherLayout makeLauncherLayout()
{
    auto screen = app_ui::screenRect();
    auto safe = app_ui::safeContentRect();
    constexpr int railW = 210;
    constexpr int gap = 20;
    const auto columns = app_ui::makeColumns(safe, {railW, 0}, gap);

    LauncherLayout layout;
    layout.screen = screen;
    layout.safe = safe;
    layout.topBar = {0, 0, screen.w, 72};
    layout.headerTitle = {24, 20, 224, 32};
    layout.headerStatus = {280, 25, 650, 24};
    layout.headerTime = {962, 25, 294, 24};
    layout.categoryRail = columns.size() == 2 ? columns[0] : app_ui::Rect{safe.x, safe.y, railW, safe.h};
    layout.tileArea = columns.size() == 2 ? columns[1] : app_ui::Rect{safe.x + railW + gap, safe.y, 0, safe.h};
    layout.columnGap = gap;
    layout.tileColumns = 3;
    layout.tileGap = 18;
    layout.tileGridX = 20;
    layout.tileGridY = 62;
    constexpr int tileRightMargin = 20;
    layout.tileW = (layout.tileArea.w - layout.tileGridX - tileRightMargin - (layout.tileGap * (layout.tileColumns - 1))) /
                   layout.tileColumns;
    layout.tileH = 184;
    layout.tileIcon = {16, 16, 48, 48};
    layout.tileTitle = {78, 25, layout.tileW - 94, 28};
    layout.tileDescription = {16, 82, layout.tileW - 32, 48};
    layout.tileStatus = {16, 148, layout.tileW - 32, 24};
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

}  // namespace launcher_view
