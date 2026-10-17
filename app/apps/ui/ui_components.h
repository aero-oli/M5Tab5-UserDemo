/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>

namespace app_ui {

enum class ButtonKind {
    Primary,
    Secondary,
    Danger,
};

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, int x, int y, const lv_font_t* font, lv_color_t color);
lv_obj_t* makeWrappedLabel(lv_obj_t* parent, const char* text, int x, int y, const lv_font_t* font, lv_color_t color);
lv_obj_t* makeFittedLabel(lv_obj_t* parent,
                          const char* text,
                          int x,
                          int y,
                          int w,
                          int h,
                          const lv_font_t* font,
                          lv_color_t color,
                          lv_label_long_mode_t longMode,
                          lv_text_align_t align);
lv_obj_t* makeScreenRoot(int width, int height);
lv_obj_t* makePanel(lv_obj_t* parent, int x, int y, int w, int h);
lv_obj_t* makeScrollableContent(lv_obj_t* parent, int x, int y, int w, int h);
lv_obj_t* makeButtonSurface(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t bg, lv_color_t border, int borderWidth, int radius);
lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int x, int y, int w, int h, ButtonKind kind);
lv_obj_t* makeTextArea(lv_obj_t* parent, const char* placeholder, int x, int y, int w, int h, bool password = false);
lv_obj_t* makeKeyboard(lv_obj_t* parent, int x, int y, int w, int h);
lv_obj_t* makeSwitch(lv_obj_t* parent, int x, int y, int w, int h, bool checked = false);
lv_obj_t* makeSlider(lv_obj_t* parent, int x, int y, int w, int h, int min, int max, int value = 0);
lv_obj_t* makeProgressBar(lv_obj_t* parent, int x, int y, int w, int h, int min, int max, int value = 0);
lv_obj_t* makeChip(lv_obj_t* parent, const char* text, int x, int y, lv_color_t color);
lv_obj_t* makeIconBadge(lv_obj_t* parent, const char* text, int x, int y, int w, int h, lv_color_t color);
lv_obj_t* makeMetricCard(lv_obj_t* parent, const char* title, const char* value, int x, int y, int w, int h);
lv_obj_t* makeEmptyState(lv_obj_t* parent, const char* title, const char* detail);
void destroyObject(lv_obj_t*& object);

}  // namespace app_ui
