/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "ui_components.h"
#include "ui_theme.h"

using namespace app_ui;

lv_obj_t* app_ui::makeLabel(lv_obj_t* parent, const char* text, int x, int y, const lv_font_t* font, lv_color_t color)
{
    auto* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t* app_ui::makeWrappedLabel(lv_obj_t* parent, const char* text, int x, int y, const lv_font_t* font, lv_color_t color)
{
    auto* label = makeLabel(parent, text, x, y, font, color);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}

lv_obj_t* app_ui::makeFittedLabel(lv_obj_t* parent,
                                  const char* text,
                                  int x,
                                  int y,
                                  int w,
                                  int h,
                                  const lv_font_t* font,
                                  lv_color_t color,
                                  lv_label_long_mode_t longMode,
                                  lv_text_align_t align)
{
    auto* label = makeLabel(parent, text, x, y, font, color);
    lv_obj_set_size(label, w, h);
    lv_label_set_long_mode(label, longMode);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

lv_obj_t* app_ui::makeScreenRoot(int width, int height)
{
    auto* root = lv_obj_create(lv_screen_active());
    lv_obj_set_align(root, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, width, height);
    lv_obj_set_style_bg_color(root, theme::color(theme::bg), 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    return root;
}

lv_obj_t* app_ui::makePanel(lv_obj_t* parent, int x, int y, int w, int h)
{
    auto* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, theme::color(theme::panel), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, theme::color(theme::border), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 16, 0);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t* app_ui::makeScrollableContent(lv_obj_t* parent, int x, int y, int w, int h)
{
    auto* content = lv_obj_create(parent);
    lv_obj_set_pos(content, x, y);
    lv_obj_set_size(content, w, h);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_bottom(content, 24, 0);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLL_CHAIN);
    return content;
}

lv_obj_t* app_ui::makeButtonSurface(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t bg, lv_color_t border, int borderWidth, int radius)
{
    auto* button = lv_button_create(parent);
    lv_obj_set_layout(button, LV_LAYOUT_NONE);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_radius(button, radius, 0);
    lv_obj_set_style_bg_color(button, bg, 0);
    lv_obj_set_style_bg_color(button, theme::color(theme::focus), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, border, 0);
    lv_obj_set_style_border_width(button, borderWidth, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_opa(button, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    return button;
}

lv_obj_t* app_ui::makeButton(lv_obj_t* parent, const char* text, int x, int y, int w, int h, ButtonKind kind)
{
    uint32_t bg = theme::blue;
    if (kind == ButtonKind::Secondary) {
        bg = theme::panelAlt;
    } else if (kind == ButtonKind::Danger) {
        bg = theme::red;
    }

    const int labelHeight = h > 28 ? 28 : h;
    auto* button = makeButtonSurface(parent, x, y, w, h, theme::color(bg), theme::color(bg), 0, 8);
    makeFittedLabel(button,
                    text,
                    12,
                    (h - labelHeight) / 2,
                    w - 24,
                    labelHeight,
                    &lv_font_montserrat_18,
                    theme::color(theme::text),
                    LV_LABEL_LONG_DOT,
                    LV_TEXT_ALIGN_CENTER);
    return button;
}

lv_obj_t* app_ui::makeTextArea(lv_obj_t* parent, const char* placeholder, int x, int y, int w, int h, bool password)
{
    auto* textArea = lv_textarea_create(parent);
    lv_obj_set_pos(textArea, x, y);
    lv_obj_set_size(textArea, w, h);
    lv_textarea_set_placeholder_text(textArea, placeholder);
    lv_textarea_set_one_line(textArea, true);
    lv_textarea_set_password_mode(textArea, password);
    lv_obj_set_style_bg_color(textArea, theme::color(theme::panelAlt), 0);
    lv_obj_set_style_bg_opa(textArea, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(textArea, theme::color(theme::border), 0);
    lv_obj_set_style_border_width(textArea, 1, 0);
    lv_obj_set_style_radius(textArea, 8, 0);
    lv_obj_set_style_text_color(textArea, theme::color(theme::text), 0);
    lv_obj_set_style_text_font(textArea, &lv_font_montserrat_18, 0);
    lv_obj_set_style_pad_left(textArea, 12, 0);
    lv_obj_set_style_pad_right(textArea, 12, 0);
    return textArea;
}

lv_obj_t* app_ui::makeKeyboard(lv_obj_t* parent, int x, int y, int w, int h)
{
    auto* keyboard = lv_keyboard_create(parent);
    lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(keyboard, x, y);
    lv_obj_set_size(keyboard, w, h);
    lv_obj_set_style_bg_color(keyboard, theme::color(theme::panel), 0);
    lv_obj_set_style_border_width(keyboard, 0, 0);
    lv_obj_set_style_pad_all(keyboard, 8, 0);
    lv_obj_set_style_pad_row(keyboard, 8, 0);
    lv_obj_set_style_pad_column(keyboard, 8, 0);
    lv_obj_set_style_bg_color(keyboard, theme::color(theme::panelAlt), LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, theme::color(theme::text), LV_PART_ITEMS);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, theme::color(theme::focus), LV_PART_ITEMS | LV_STATE_PRESSED);
    return keyboard;
}

lv_obj_t* app_ui::makeSwitch(lv_obj_t* parent, int x, int y, int w, int h, bool checked)
{
    auto* sw = lv_switch_create(parent);
    lv_obj_set_pos(sw, x, y);
    lv_obj_set_size(sw, w, h);
    lv_obj_set_style_bg_color(sw, theme::color(theme::panelAlt), 0);
    lv_obj_set_style_bg_color(sw, theme::color(theme::blue), LV_STATE_CHECKED);
    const auto checkedIndicator = static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | static_cast<lv_style_selector_t>(LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, theme::color(theme::blue), checkedIndicator);
    lv_obj_set_style_bg_color(sw, theme::color(theme::text), LV_PART_KNOB);
    lv_obj_set_style_border_color(sw, theme::color(theme::border), 0);
    lv_obj_set_style_border_width(sw, 1, 0);
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    return sw;
}

lv_obj_t* app_ui::makeSlider(lv_obj_t* parent, int x, int y, int w, int h, int min, int max, int value)
{
    auto* slider = lv_slider_create(parent);
    lv_obj_set_pos(slider, x, y);
    lv_obj_set_size(slider, w, h);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, theme::color(theme::panelAlt), 0);
    lv_obj_set_style_bg_color(slider, theme::color(theme::blue), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, theme::color(theme::text), LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, theme::color(theme::border), 0);
    lv_obj_set_style_border_width(slider, 1, 0);
    lv_obj_set_style_radius(slider, 8, 0);
    return slider;
}

lv_obj_t* app_ui::makeProgressBar(lv_obj_t* parent, int x, int y, int w, int h, int min, int max, int value)
{
    auto* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, min, max);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, theme::color(theme::panelAlt), 0);
    lv_obj_set_style_bg_color(bar, theme::color(theme::blue), LV_PART_INDICATOR);
    lv_obj_set_style_border_color(bar, theme::color(theme::border), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 8, 0);
    return bar;
}

lv_obj_t* app_ui::makeChip(lv_obj_t* parent, const char* text, int x, int y, lv_color_t color)
{
    auto* chip = lv_obj_create(parent);
    lv_obj_set_pos(chip, x, y);
    lv_obj_set_size(chip, 96, 32);
    lv_obj_set_style_bg_color(chip, color, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 16, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    auto* label = lv_label_create(chip);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, 76);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, theme::color(theme::text), 0);
    lv_obj_center(label);
    return chip;
}

lv_obj_t* app_ui::makeIconBadge(lv_obj_t* parent, const char* text, int x, int y, int w, int h, lv_color_t color)
{
    auto* badge = lv_obj_create(parent);
    lv_obj_set_pos(badge, x, y);
    lv_obj_set_size(badge, w, h);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_set_style_bg_color(badge, color, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    const int labelHeight = h > 24 ? 24 : h;
    makeFittedLabel(badge,
                    text,
                    4,
                    (h - labelHeight) / 2,
                    w - 8,
                    labelHeight,
                    &lv_font_montserrat_18,
                    theme::color(theme::text),
                    LV_LABEL_LONG_DOT,
                    LV_TEXT_ALIGN_CENTER);
    return badge;
}

lv_obj_t* app_ui::makeMetricCard(lv_obj_t* parent, const char* title, const char* value, int x, int y, int w, int h)
{
    auto* card = makePanel(parent, x, y, w, h);
    lv_obj_set_style_bg_color(card, theme::color(theme::surfaceRaised), 0);
    auto* titleLabel = makeLabel(card, title, 0, 0, &lv_font_montserrat_16, theme::color(theme::muted));
    lv_obj_set_width(titleLabel, w - 32);
    lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
    auto* valueLabel = makeLabel(card, value, 0, 42, &lv_font_montserrat_28, theme::color(theme::text));
    lv_obj_set_width(valueLabel, w - 32);
    lv_label_set_long_mode(valueLabel, LV_LABEL_LONG_DOT);
    return card;
}

lv_obj_t* app_ui::makeEmptyState(lv_obj_t* parent, const char* title, const char* detail)
{
    auto* panel = makePanel(parent, 0, 0, lv_obj_get_width(parent), lv_obj_get_height(parent));
    auto* titleLabel = makeLabel(panel, title, 32, 220, &lv_font_montserrat_24, theme::color(theme::text));
    lv_obj_set_width(titleLabel, lv_obj_get_width(panel) - 64);
    auto* detailLabel = makeLabel(panel, detail, 32, 270, &lv_font_montserrat_18, theme::color(theme::muted));
    lv_obj_set_width(detailLabel, lv_obj_get_width(panel) - 64);
    lv_label_set_long_mode(detailLabel, LV_LABEL_LONG_WRAP);
    return panel;
}

void app_ui::destroyObject(lv_obj_t*& object)
{
    if (object == nullptr) {
        return;
    }
    lv_obj_delete(object);
    object = nullptr;
}
