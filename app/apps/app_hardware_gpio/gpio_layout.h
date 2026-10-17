/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <apps/ui/ui_layout.h>

namespace app_hardware_gpio {

struct GpioLayout {
    app_ui::Rect content;
    app_ui::Rect statusText;
    app_ui::Rect errorText;
    app_ui::Rect releaseAll;
    app_ui::Rect grid;
    int cols = 4;
    int cardW = 296;
    int cardH = 180;
    int gap = 16;
    int scrollContentHeight = 0;
    app_ui::Rect title;
    app_ui::Rect role;
    app_ui::Rect state;
    app_ui::Rect toggle;
    app_ui::Rect action;
};

inline GpioLayout makeGpioLayout(int pinCount)
{
    GpioLayout layout;
    layout.content = {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
    layout.statusText = {0, 0, 980, 28};
    layout.errorText = {0, 34, 980, 28};
    layout.releaseAll = {layout.content.w - 200, 0, 200, 48};
    layout.grid = {0, 80, layout.content.w, layout.content.h - 80};
    layout.cardW = (layout.grid.w - (layout.cols - 1) * layout.gap) / layout.cols;
    const int innerW = layout.cardW - 36;
    layout.title = {0, 0, innerW, 24};
    layout.role = {0, 28, innerW, 20};
    layout.state = {0, 56, innerW - 96, 28};
    layout.toggle = {innerW - 76, 52, 72, 38};
    layout.action = {0, 96, innerW, 44};
    const int rows = pinCount > 0 ? (pinCount + layout.cols - 1) / layout.cols : 0;
    layout.scrollContentHeight = rows > 0 ? rows * (layout.cardH + layout.gap) - layout.gap + 24 : 0;
    return layout;
}

inline app_ui::Rect gpioCardRect(const GpioLayout& layout, int index)
{
    return {(index % layout.cols) * (layout.cardW + layout.gap),
            (index / layout.cols) * (layout.cardH + layout.gap), layout.cardW, layout.cardH};
}

}  // namespace app_hardware_gpio
