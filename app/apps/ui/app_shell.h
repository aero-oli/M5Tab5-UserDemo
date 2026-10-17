/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <functional>
#include <string>
#include <vector>
#include <lvgl.h>
#include "ui_layout.h"

namespace app_ui {

class AppShell {
public:
    AppShell(std::string title, std::function<void()> onBack);
    ~AppShell();

    void init();
    lv_obj_t* root() const;
    lv_obj_t* content() const;
    void setTitle(const std::string& title);
    void setStatusChips(const std::vector<std::string>& chips);
    // Replaces the chips with an action that remains reachable while editing.
    void setHeaderAction(const std::string& label, std::function<void()> action);
    void setContentScrollable(bool scrollable);
    void bindTextInput(lv_obj_t* field);
    void confirm(std::string title, std::string message, std::function<void()> onConfirm,
                 std::function<void()> onCancel = {});
    bool dismissConfirmation();

private:
    void openTextEditor(lv_obj_t* field);
    void closeTextEditor(bool save);
    std::string _title;
    std::function<void()> _onBack;
    lv_obj_t* _root                = nullptr;
    lv_obj_t* _header              = nullptr;
    lv_obj_t* _titleLabel          = nullptr;
    lv_obj_t* _chipArea            = nullptr;
    lv_obj_t* _content             = nullptr;
    lv_obj_t* _editor              = nullptr;
    lv_obj_t* _editorTitle         = nullptr;
    lv_obj_t* _draft               = nullptr;
    lv_obj_t* _keyboard            = nullptr;
    lv_obj_t* _editedField         = nullptr;
    lv_obj_t* _confirmation        = nullptr;
    lv_obj_t* _confirmationTitle   = nullptr;
    lv_obj_t* _confirmationMessage = nullptr;
    std::function<void()> _onConfirm;
    std::function<void()> _onCancel;
    std::function<void()> _headerAction;
};

}  // namespace app_ui
