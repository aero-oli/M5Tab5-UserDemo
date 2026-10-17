/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_shell.h"
#include <hal/hal.h>
#include <smooth_lvgl.h>
#include "app_shell_layout.h"
#include "ui_theme.h"
#include "ui_components.h"

using namespace app_ui;

AppShell::AppShell(std::string title, std::function<void()> onBack) : _title(std::move(title)), _onBack(std::move(onBack)) {}

AppShell::~AppShell()
{
    LvglLockGuard lock;
    destroyObject(_root);
}

void AppShell::init()
{
    LvglLockGuard lock;
    const auto layout = makeAppShellLayout();
    _root = makeScreenRoot(GetHAL()->getDisplayWidth(), GetHAL()->getDisplayHeight());

    _header = lv_obj_create(_root);
    lv_obj_set_pos(_header, layout.header.x, layout.header.y);
    lv_obj_set_size(_header, layout.header.w, layout.header.h);
    lv_obj_set_style_bg_color(_header, theme::color(theme::panel), 0);
    lv_obj_set_style_border_width(_header, 0, 0);
    lv_obj_set_style_radius(_header, 0, 0);
    lv_obj_set_style_pad_all(_header, 0, 0);
    lv_obj_clear_flag(_header, LV_OBJ_FLAG_SCROLLABLE);

    auto* back = makeButton(_header, "Back", layout.backButton.x, layout.backButton.y, layout.backButton.w,
                            layout.backButton.h, ButtonKind::Secondary);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t* event) {
            auto* shell = static_cast<AppShell*>(lv_event_get_user_data(event));
            if (shell->dismissConfirmation()) return;
            if (shell->_onBack) {
                shell->_onBack();
            }
        },
        LV_EVENT_CLICKED,
        this);

    _titleLabel = makeLabel(_header, _title.c_str(), layout.title.x, layout.title.y, &lv_font_montserrat_24,
                            theme::color(theme::text));
    lv_obj_set_width(_titleLabel, layout.title.w);
    lv_label_set_long_mode(_titleLabel, LV_LABEL_LONG_DOT);
    _chipArea = lv_obj_create(_header);
    lv_obj_set_pos(_chipArea, layout.chipArea.x, layout.chipArea.y);
    lv_obj_set_size(_chipArea, layout.chipArea.w, layout.chipArea.h);
    lv_obj_set_style_bg_opa(_chipArea, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_chipArea, 0, 0);
    lv_obj_set_style_pad_all(_chipArea, 0, 0);
    lv_obj_clear_flag(_chipArea, LV_OBJ_FLAG_SCROLLABLE);

    _content = lv_obj_create(_root);
    lv_obj_set_pos(_content, layout.content.x, layout.content.y);
    lv_obj_set_size(_content, layout.content.w, layout.content.h);
    lv_obj_set_style_bg_opa(_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_style_radius(_content, 0, 0);
    lv_obj_set_style_pad_all(_content, 0, 0);
    lv_obj_clear_flag(_content, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* AppShell::root() const
{
    return _root;
}

lv_obj_t* AppShell::content() const
{
    return _content;
}

void AppShell::setTitle(const std::string& title)
{
    _title = title;
    if (_titleLabel != nullptr) {
        lv_label_set_text(_titleLabel, _title.c_str());
    }
}

void AppShell::setStatusChips(const std::vector<std::string>& chips)
{
    if (_chipArea == nullptr) {
        return;
    }
    lv_obj_clean(_chipArea);
    const auto layout = makeAppShellLayout();
    const int maxChips = maxAppShellChips(layout);
    for (int i = 0; i < static_cast<int>(chips.size()); ++i) {
        if (i >= maxChips) {
            break;
        }
        const auto rect = appShellChipRect(layout, i);
        makeChip(_chipArea, chips[static_cast<size_t>(i)].c_str(), rect.x, rect.y, theme::color(theme::blue));
    }
}

void AppShell::setContentScrollable(bool scrollable)
{
    if (_content == nullptr) {
        return;
    }
    if (scrollable) {
        lv_obj_add_flag(_content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(_content, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(_content, LV_SCROLLBAR_MODE_AUTO);
    } else {
        lv_obj_clear_flag(_content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(_content, LV_SCROLLBAR_MODE_OFF);
    }
}

void AppShell::bindTextInput(lv_obj_t* field)
{
    lv_obj_add_event_cb(field, [](lv_event_t* event) {
        auto* shell = static_cast<AppShell*>(lv_event_get_user_data(event));
        shell->openTextEditor(static_cast<lv_obj_t*>(lv_event_get_target(event)));
    }, LV_EVENT_CLICKED, this);
}

void AppShell::confirm(std::string title, std::string message, std::function<void()> onConfirm,
                       std::function<void()> onCancel)
{
    LvglLockGuard lock;
    if (_confirmation == nullptr) {
        const auto safe = makeAppShellLayout().content;
        _confirmation = makePanel(_root, safe.x, safe.y, safe.w, safe.h);
        lv_obj_set_style_pad_all(_confirmation, 0, 0);
        _confirmationTitle = makeFittedLabel(_confirmation, "", 32, 32, safe.w - 64, 40,
            &lv_font_montserrat_28, theme::color(theme::text), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
        _confirmationMessage = makeFittedLabel(_confirmation, "", 32, 104, safe.w - 64, 220,
            &lv_font_montserrat_20, theme::color(theme::amber), LV_LABEL_LONG_WRAP, LV_TEXT_ALIGN_LEFT);
        auto* cancel = makeButton(_confirmation, "Cancel", 32, safe.h - 88, 180, 56, ButtonKind::Secondary);
        auto* accept = makeButton(_confirmation, "Confirm", safe.w - 212, safe.h - 88, 180, 56, ButtonKind::Danger);
        lv_obj_add_event_cb(cancel, [](lv_event_t* event) {
            static_cast<AppShell*>(lv_event_get_user_data(event))->dismissConfirmation();
        }, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(accept, [](lv_event_t* event) {
            auto* shell = static_cast<AppShell*>(lv_event_get_user_data(event));
            auto callback = std::move(shell->_onConfirm);
            shell->_onCancel = {};
            shell->dismissConfirmation();
            if (callback) callback();
        }, LV_EVENT_CLICKED, this);
    }
    _onConfirm = std::move(onConfirm);
    _onCancel = std::move(onCancel);
    lv_label_set_text(_confirmationTitle, title.c_str());
    lv_label_set_text(_confirmationMessage, message.c_str());
    lv_obj_add_flag(_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_confirmation, LV_OBJ_FLAG_HIDDEN);
}

bool AppShell::dismissConfirmation()
{
    if (!_confirmation || lv_obj_has_flag(_confirmation, LV_OBJ_FLAG_HIDDEN)) return false;
    // Keep the dialog objects alive until nested LVGL event dispatch has unwound.
    lv_obj_add_flag(_confirmation, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_content, LV_OBJ_FLAG_HIDDEN);
    _onConfirm = {};
    auto callback = std::move(_onCancel);
    if (callback) callback();
    return true;
}

void AppShell::openTextEditor(lv_obj_t* field)
{
    const auto layout = makeAppShellLayout();
    if (_editor == nullptr) {
        _editor = makePanel(_root, layout.content.x, layout.content.y, layout.content.w, layout.content.h);
        lv_obj_set_style_pad_all(_editor, 0, 0);
        _editorTitle = makeFittedLabel(_editor, "", 24, 20, layout.content.w - 48, 32,
                                      &lv_font_montserrat_24, theme::color(theme::text),
                                      LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
        _draft = makeTextArea(_editor, "", 24, 72, layout.content.w - 48, 64);
        _keyboard = makeKeyboard(_editor, 16, 200, layout.content.w - 32, layout.content.h - 216);
        auto onEditorEvent = [](lv_event_t* event) {
            const auto code = lv_event_get_code(event);
            if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
                auto* shell = static_cast<AppShell*>(lv_event_get_user_data(event));
                shell->closeTextEditor(code == LV_EVENT_READY);
            }
        };
        lv_obj_add_event_cb(_keyboard, onEditorEvent, LV_EVENT_ALL, this);
        lv_obj_add_event_cb(_draft, onEditorEvent, LV_EVENT_READY, this);
    }
    _editedField = field;
    const char* placeholder = lv_textarea_get_placeholder_text(field);
    lv_label_set_text(_editorTitle, placeholder && placeholder[0] ? placeholder : _title.c_str());
    lv_textarea_set_password_mode(_draft, lv_textarea_get_password_mode(field));
    lv_textarea_set_max_length(_draft, lv_textarea_get_max_length(field));
    lv_textarea_set_accepted_chars(_draft, lv_textarea_get_accepted_chars(field));
    lv_textarea_set_text(_draft, lv_textarea_get_text(field));
    lv_textarea_set_cursor_pos(_draft, LV_TEXTAREA_CURSOR_LAST);
    lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(_keyboard, _draft);
    lv_obj_add_flag(_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_editor, LV_OBJ_FLAG_HIDDEN);
}

void AppShell::closeTextEditor(bool save)
{
    if (save && _editedField != nullptr) {
        lv_textarea_set_text(_editedField, lv_textarea_get_text(_draft));
    }
    // Keep the keyboard alive until LVGL's nested Ready/Cancel dispatch finishes.
    lv_keyboard_set_textarea(_keyboard, nullptr);
    lv_textarea_set_text(_draft, "");
    _editedField = nullptr;
    lv_obj_add_flag(_editor, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_content, LV_OBJ_FLAG_HIDDEN);
}
