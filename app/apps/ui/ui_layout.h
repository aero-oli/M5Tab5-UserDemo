/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <vector>

namespace app_ui {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

inline Rect screenRect()
{
    return {0, 0, 1280, 720};
}

inline Rect safeContentRect()
{
    return {24, 96, 1232, 600};
}

inline bool contains(const Rect& parent, const Rect& child)
{
    return child.x >= parent.x && child.y >= parent.y && child.x + child.w <= parent.x + parent.w &&
           child.y + child.h <= parent.y + parent.h;
}

inline bool containsLocal(const Rect& parent, const Rect& child)
{
    return child.x >= 0 && child.y >= 0 && child.x + child.w <= parent.w && child.y + child.h <= parent.h;
}

inline std::vector<Rect> makeGrid(const Rect& area, int cols, int rows, int gap, int tileW, int tileH)
{
    std::vector<Rect> rects;
    if (cols <= 0 || rows <= 0 || tileW <= 0 || tileH <= 0) {
        return rects;
    }

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            rects.push_back({area.x + col * (tileW + gap), area.y + row * (tileH + gap), tileW, tileH});
        }
    }
    return rects;
}

inline std::vector<Rect> makeColumns(const Rect& area, const std::vector<int>& widths, int gap)
{
    std::vector<Rect> rects;
    if (area.w <= 0 || area.h <= 0 || widths.empty() || gap < 0) {
        return rects;
    }

    int fixedWidth = 0;
    int flexibleIndex = -1;
    for (size_t i = 0; i < widths.size(); ++i) {
        if (widths[i] <= 0) {
            if (flexibleIndex >= 0) {
                return {};
            }
            flexibleIndex = static_cast<int>(i);
            continue;
        }
        fixedWidth += widths[i];
    }

    const int totalGap = gap * static_cast<int>(widths.size() - 1);
    int flexibleWidth = 0;
    if (flexibleIndex >= 0) {
        flexibleWidth = area.w - fixedWidth - totalGap;
        if (flexibleWidth <= 0) {
            return {};
        }
    } else if (fixedWidth + totalGap > area.w) {
        return {};
    }

    int x = area.x;
    rects.reserve(widths.size());
    for (size_t i = 0; i < widths.size(); ++i) {
        const int width = static_cast<int>(i) == flexibleIndex ? flexibleWidth : widths[i];
        rects.push_back({x, area.y, width, area.h});
        x += width + gap;
    }
    return rects;
}

}  // namespace app_ui
