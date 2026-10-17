/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_pps.h"
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace mooncake;

AppPps::AppPps()
{
    setAppInfo().name = "Module PPS";
}

void AppPps::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppPps::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _controller = std::make_unique<app_pps::PpsController>();
    _view       = std::make_unique<app_pps::PpsView>(*_controller, [&]() { close(); });
    _view->init();
}

void AppPps::onRunning()
{
    uint32_t now = GetHAL()->millis();
    _controller->update(now);
    _view->update(now);
}

void AppPps::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _view.reset();
    _controller.reset();
}
