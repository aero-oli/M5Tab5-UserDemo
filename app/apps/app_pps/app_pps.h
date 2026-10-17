/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "pps_controller.h"
#include "pps_view.h"
#include <memory>
#include <mooncake.h>

class AppPps : public mooncake::AppAbility {
public:
    AppPps();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<app_pps::PpsController> _controller;
    std::unique_ptr<app_pps::PpsView> _view;
};
