/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "ui_layout.h"

namespace app_ui {

struct AppShellLayout {
    Rect root;
    Rect header;
    Rect backButton;
    Rect title;
    Rect chipArea;
    Rect chip;
    Rect content;
    int chipGap = 8;
};

inline AppShellLayout makeAppShellLayout()
{
    const auto screen = screenRect();
    const auto safe   = safeContentRect();

    AppShellLayout layout;
    layout.root       = screen;
    layout.header     = {0, 0, screen.w, 72};
    layout.backButton = {24, 16, 96, 40};
    layout.title      = {144, 20, 560, 32};
    layout.chipArea   = {736, 14, 400, 44};
    layout.chip       = {0, 2, 96, 32};
    layout.content    = safe;
    return layout;
}

inline int maxAppShellChips(const AppShellLayout& layout)
{
    if (layout.chip.w <= 0 || layout.chipArea.w <= 0 || layout.chipGap < 0) {
        return 0;
    }
    return (layout.chipArea.w + layout.chipGap) / (layout.chip.w + layout.chipGap);
}

inline Rect appShellChipRect(const AppShellLayout& layout, int index)
{
    if (index < 0 || index >= maxAppShellChips(layout)) {
        return {};
    }
    return {
        layout.chip.x + index * (layout.chip.w + layout.chipGap),
        layout.chip.y,
        layout.chip.w,
        layout.chip.h,
    };
}

}  // namespace app_ui
