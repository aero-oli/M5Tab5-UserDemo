/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <memory>
#include <mooncake.h>
#include <apps/ui/app_shell.h>

class AppTemplate : public mooncake::AppAbility {
public:
    AppTemplate();

    void onOpen() override;
    void onClose() override;

private:
    std::unique_ptr<app_ui::AppShell> _shell;
};
