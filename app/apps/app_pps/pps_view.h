/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "pps_controller.h"
#include <functional>
#include <memory>
#include <lvgl.h>
#include <apps/ui/app_shell.h>

namespace app_pps {

class PpsView {
public:
    PpsView(PpsController& controller, std::function<void()> onClose);
    ~PpsView();

    void init();
    void update(uint32_t nowMs);

private:
    PpsController& _controller;
    std::function<void()> _onClose;
    uint32_t _lastUiUpdateMs = 0;
    bool _syncingControls = false;
    bool _voltageInvalid = false;
    bool _currentInvalid = false;
    float _lastInputVoltage = -1;
    float _lastInputCurrent = -1;

    std::unique_ptr<app_ui::AppShell> _shell;
    lv_obj_t* _root = nullptr;
    lv_obj_t* _status = nullptr;
    lv_obj_t* _mode = nullptr;
    lv_obj_t* _vin = nullptr;
    lv_obj_t* _temp = nullptr;
    lv_obj_t* _voltageReadback = nullptr;
    lv_obj_t* _currentReadback = nullptr;
    lv_obj_t* _powerReadback = nullptr;
    lv_obj_t* _voltageSet = nullptr;
    lv_obj_t* _currentSet = nullptr;
    lv_obj_t* _diagnostics = nullptr;
    lv_obj_t* _lastError = nullptr;
    lv_obj_t* _voltageSlider = nullptr;
    lv_obj_t* _currentSlider = nullptr;
    lv_obj_t* _voltageInput = nullptr;
    lv_obj_t* _currentInput = nullptr;
    lv_obj_t* _outputSwitch = nullptr;
    lv_obj_t* _controlsPanel = nullptr;
    lv_obj_t* _applyButton = nullptr;
    lv_obj_t* _preset33 = nullptr;
    lv_obj_t* _preset5 = nullptr;
    lv_obj_t* _preset9 = nullptr;
    lv_obj_t* _preset12 = nullptr;
    lv_obj_t* _presetCustom = nullptr;

    void refreshAll(uint32_t nowMs);
    void refreshPendingLabels();
    void syncInputs(bool resetInvalid = false);
    void createHeader();
    void createTelemetry();
    void createControls();
    void createDiagnostics();
    void createButton(lv_obj_t*& obj, const char* text, int x, int y, int w, int h, lv_color_t color);

    static void onBack(lv_event_t* event);
    static void onApply(lv_event_t* event);
    static void onEmergencyOff(lv_event_t* event);
    static void onOutputSwitch(lv_event_t* event);
    static void onSliderChanged(lv_event_t* event);
    static void onInputChanged(lv_event_t* event);
    static void onPreset(lv_event_t* event);
};

}  // namespace app_pps
