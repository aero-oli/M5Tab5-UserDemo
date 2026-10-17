/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "pps_view.h"
#include <cstdio>
#include <cstring>
#include <hal/hal.h>
#include <apps/utils/audio/audio.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>
#include "pps_layout.h"

using namespace app_pps;

namespace {

void setLabel(lv_obj_t* label, const char* text)
{
    if (std::strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

void setFloatLabel(lv_obj_t* label, const char* format, float value)
{
    // LVGL's configured formatter does not support floats on either platform.
    char text[64];
    std::snprintf(text, sizeof(text), format, static_cast<double>(value));
    setLabel(label, text);
}

}  // namespace

PpsView::PpsView(PpsController& controller, std::function<void()> onClose) : _controller(controller), _onClose(onClose) {}

PpsView::~PpsView()
{
    _root = nullptr;
    _shell.reset();
}

void PpsView::init()
{
    _shell = std::make_unique<app_ui::AppShell>("Module13.2 PPS", [&]() { _onClose(); });
    _shell->init();

    LvglLockGuard lock;
    _shell->setStatusChips({"I2C 0x35", "100W"});
    _root = _shell->content();

    createHeader();
    createTelemetry();
    createControls();
    createDiagnostics();
    refreshAll(GetHAL()->millis());
}

void PpsView::update(uint32_t nowMs)
{
    if (nowMs - _lastUiUpdateMs < 200) {
        return;
    }

    LvglLockGuard lock;
    refreshAll(nowMs);
    _lastUiUpdateMs = nowMs;
}

void PpsView::createHeader()
{
    const auto layout = makePpsLayout();
    lv_obj_t* panel =
        app_ui::makePanel(_root, layout.statusStrip.x, layout.statusStrip.y, layout.statusStrip.w, layout.statusStrip.h);
    auto makeStatusLabel = [panel](const char* text, const app_ui::Rect& rect) {
        return app_ui::makeFittedLabel(panel, text, rect.x, rect.y, rect.w, rect.h, &lv_font_montserrat_18,
                                      lv_color_hex(0xAEB8C4), LV_LABEL_LONG_DOT, LV_TEXT_ALIGN_LEFT);
    };
    _status = makeStatusLabel("Checking PPS...", layout.connectionText);
    _mode = makeStatusLabel("Mode: --", layout.modeText);
    _vin = makeStatusLabel("VIN: --", layout.vinText);
    _temp = makeStatusLabel("Temp: --", layout.temperatureText);
}

void PpsView::createTelemetry()
{
    const auto layout = makePpsLayout();
    lv_obj_t* panel = app_ui::makePanel(_root,
                                        layout.telemetryPanel.x,
                                        layout.telemetryPanel.y,
                                        layout.telemetryPanel.w,
                                        layout.telemetryPanel.h);
    app_ui::makeLabel(panel, "Live Output", 0, 0, &lv_font_montserrat_22, lv_color_hex(0xDDE6EE));
    _voltageReadback = app_ui::makeLabel(panel, "0.00 V", 0, 56, &lv_font_montserrat_32, lv_color_hex(0x8FD7FF));
    _currentReadback = app_ui::makeLabel(panel, "0.000 A", 300, 56, &lv_font_montserrat_32, lv_color_hex(0x9AF0BD));
    _powerReadback = app_ui::makeLabel(panel, "0.00 W", 0, 158, &lv_font_montserrat_32, lv_color_hex(0xFFE08A));
}

void PpsView::createControls()
{
    const auto layout = makePpsLayout();
    _controlsPanel =
        app_ui::makePanel(_root, layout.controlsPanel.x, layout.controlsPanel.y, layout.controlsPanel.w, layout.controlsPanel.h);
    lv_obj_t* panel = _controlsPanel;
    app_ui::makeLabel(panel, "Controls", 0, 0, &lv_font_montserrat_22, lv_color_hex(0xDDE6EE));

    app_ui::makeLabel(panel, "Output", 0, 58, &lv_font_montserrat_20, lv_color_hex(0xC5CFDA));
    _outputSwitch = app_ui::makeSwitch(panel, layout.outputSwitch.x, layout.outputSwitch.y, layout.outputSwitch.w, layout.outputSwitch.h);
    lv_obj_add_event_cb(_outputSwitch, PpsView::onOutputSwitch, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* off = nullptr;
    createButton(off,
                 "Emergency Off",
                 layout.emergencyOffButton.x,
                 layout.emergencyOffButton.y,
                 layout.emergencyOffButton.w,
                 layout.emergencyOffButton.h,
                 lv_color_hex(0x9F2D35));
    lv_obj_add_event_cb(off, PpsView::onEmergencyOff, LV_EVENT_CLICKED, this);

    _voltageSet = app_ui::makeLabel(panel, "Set V: 5.0 V", 0, 128, &lv_font_montserrat_20, lv_color_hex(0xF3F7FB));
    _voltageSlider = app_ui::makeSlider(panel, layout.voltageSlider.x, layout.voltageSlider.y, layout.voltageSlider.w, layout.voltageSlider.h, 5, 300, 50);
    lv_obj_add_event_cb(_voltageSlider, PpsView::onSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    _currentSet = app_ui::makeLabel(panel, "Set A: 1.00 A", 0, 214, &lv_font_montserrat_20, lv_color_hex(0xF3F7FB));
    _currentSlider = app_ui::makeSlider(panel, layout.currentSlider.x, layout.currentSlider.y, layout.currentSlider.w, layout.currentSlider.h, 0, 500, 100);
    lv_obj_add_event_cb(_currentSlider, PpsView::onSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    createButton(_preset33, "3.3V", layout.preset33Button.x, layout.preset33Button.y, layout.preset33Button.w, layout.preset33Button.h, lv_color_hex(0x3B556C));
    createButton(_preset5, "5V", layout.preset5Button.x, layout.preset5Button.y, layout.preset5Button.w, layout.preset5Button.h, lv_color_hex(0x3B556C));
    createButton(_preset9, "9V", layout.preset9Button.x, layout.preset9Button.y, layout.preset9Button.w, layout.preset9Button.h, lv_color_hex(0x3B556C));
    createButton(_preset12, "12V", layout.preset12Button.x, layout.preset12Button.y, layout.preset12Button.w, layout.preset12Button.h, lv_color_hex(0x3B556C));
    createButton(_presetCustom,
                 "Last",
                 layout.presetCustomButton.x,
                 layout.presetCustomButton.y,
                 layout.presetCustomButton.w,
                 layout.presetCustomButton.h,
                 lv_color_hex(0x3B556C));
    lv_obj_add_event_cb(_preset33, PpsView::onPreset, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(_preset5, PpsView::onPreset, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(_preset9, PpsView::onPreset, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(_preset12, PpsView::onPreset, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(_presetCustom, PpsView::onPreset, LV_EVENT_CLICKED, this);

    createButton(_applyButton, "Apply", layout.applyButton.x, layout.applyButton.y, layout.applyButton.w, layout.applyButton.h, lv_color_hex(0x276749));
    lv_obj_add_event_cb(_applyButton, PpsView::onApply, LV_EVENT_CLICKED, this);
}

void PpsView::createDiagnostics()
{
    const auto layout = makePpsLayout();
    lv_obj_t* panel = app_ui::makePanel(_root,
                                        layout.diagnosticsPanel.x,
                                        layout.diagnosticsPanel.y,
                                        layout.diagnosticsPanel.w,
                                        layout.diagnosticsPanel.h);
    app_ui::makeLabel(panel, "Diagnostics", 0, 0, &lv_font_montserrat_22, lv_color_hex(0xDDE6EE));
    _diagnostics = app_ui::makeLabel(panel,
                                     "Module: --\nUID: --\nI2C: 0x35\nData: --\nLast poll: --",
                                     0,
                                     46,
                                     &lv_font_montserrat_18,
                                     lv_color_hex(0xC5CFDA));
    _lastError = app_ui::makeLabel(_root, "", layout.errorLabel.x, layout.errorLabel.y, &lv_font_montserrat_18, lv_color_hex(0xFFB3B3));
    lv_obj_set_width(_lastError, layout.errorLabel.w);
}

void PpsView::createButton(lv_obj_t*& obj, const char* text, int x, int y, int w, int h, lv_color_t color)
{
    obj = app_ui::makeButtonSurface(_controlsPanel == nullptr ? _root : _controlsPanel,
                                    x,
                                    y,
                                    w,
                                    h,
                                    color,
                                    color,
                                    0,
                                    8);
    const int labelHeight = h > 28 ? 28 : h;
    app_ui::makeFittedLabel(obj,
                            text,
                            12,
                            (h - labelHeight) / 2,
                            w - 24,
                            labelHeight,
                            &lv_font_montserrat_18,
                            lv_color_hex(0xFFFFFF),
                            LV_LABEL_LONG_DOT,
                            LV_TEXT_ALIGN_CENTER);
}

void PpsView::refreshAll(uint32_t nowMs)
{
    const auto state = _controller.state();
    const auto& telemetry = state.telemetry;

    setLabel(_status, state.output == OutputState::Unknown ? "Output unknown - may be live" :
                               state.output == OutputState::Enabled ? "Output On" : "Output Off confirmed");
    const auto statusColor = state.output == OutputState::Disabled ? lv_color_hex(0x9AF0BD) :
                             state.output == OutputState::Enabled ? lv_color_hex(0xFFE08A) : lv_color_hex(0xFFB3B3);
    if (!lv_color_eq(lv_obj_get_style_text_color(_status, LV_PART_MAIN), statusColor)) {
        lv_obj_set_style_text_color(_status, statusColor, 0);
    }

    if (telemetry.connected && state.telemetryFresh) {
        char mode[40];
        std::snprintf(mode, sizeof(mode), "Mode: %s", pps::PpsDevice::modeName(telemetry.mode));
        setLabel(_mode, mode);
        setFloatLabel(_vin, "VIN: %.2f V", telemetry.vin);
        setFloatLabel(_temp, "Temp: %.1f C", telemetry.temperature);
        setFloatLabel(_voltageReadback, "%.2f V", telemetry.voutReadback);
        setFloatLabel(_currentReadback, "%.3f A", telemetry.ioutReadback);
        setFloatLabel(_powerReadback, "%.2f W", telemetry.outputPower);
    } else {
        setLabel(_mode, "Mode: unknown");
        setLabel(_vin, "VIN: --");
        setLabel(_temp, "Temp: --");
        setLabel(_voltageReadback, "-- V");
        setLabel(_currentReadback, "-- A");
        setLabel(_powerReadback, "-- W");
    }

    _syncingControls = true;
    if (state.output == OutputState::Enabled) {
        lv_obj_add_state(_outputSwitch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(_outputSwitch, LV_STATE_CHECKED);
    }
    if (state.output == OutputState::Enabled || PpsController::canEnable(state)) {
        lv_obj_remove_state(_outputSwitch, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(_outputSwitch, LV_STATE_DISABLED);
    }
    lv_slider_set_value(_voltageSlider, static_cast<int>(state.pendingVoltage * 10.0f), LV_ANIM_OFF);
    lv_slider_set_value(_currentSlider, static_cast<int>(state.pendingCurrent * 100.0f), LV_ANIM_OFF);
    _syncingControls = false;
    refreshPendingLabels();

    if (PpsController::canApply(state)) {
        lv_obj_remove_state(_applyButton, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(_applyButton, LV_STATE_DISABLED);
    }

    uint32_t pollAge = state.hasTelemetry ? nowMs - state.lastGoodPollMs : 0;
    char diagnostics[256];
    std::snprintf(diagnostics, sizeof(diagnostics), "Module: %s0x%04X\nUID: %s\nI2C: 0x%02X\nData: %u\n%s: %ums",
                          state.identityValid ? "" : "-- / ", state.identity.moduleId,
                          state.identityValid ? state.identity.uid.c_str() : "--", state.identity.i2cAddress,
                          static_cast<unsigned>(telemetry.dataFlag), state.hasTelemetry ? "Last good" : "No telemetry",
                          static_cast<unsigned>(pollAge));
    setLabel(_diagnostics, diagnostics);

    const char* error = !state.lastCommandError.empty() ? state.lastCommandError.c_str() :
                        !telemetry.lastError.empty() ? telemetry.lastError.c_str() :
                        state.commandPending ? state.commandStatus.c_str() :
                        !state.hasTelemetry ? "Checking PPS..." : "";
    setLabel(_lastError, error);
}

void PpsView::refreshPendingLabels()
{
    const auto& state = _controller.state();
    setFloatLabel(_voltageSet, "Set V: %.1f V", state.pendingVoltage);
    setFloatLabel(_currentSet, "Set A: %.2f A", state.pendingCurrent);
}

void PpsView::onBack(lv_event_t* event)
{
    auto view = static_cast<PpsView*>(lv_event_get_user_data(event));
    audio::play_next_tone_progression();
    view->_onClose();
}

void PpsView::onApply(lv_event_t* event)
{
    auto view = static_cast<PpsView*>(lv_event_get_user_data(event));
    audio::play_next_tone_progression();
    view->_controller.applySetpoints(GetHAL()->millis());
    view->refreshAll(GetHAL()->millis());
}

void PpsView::onEmergencyOff(lv_event_t* event)
{
    auto view = static_cast<PpsView*>(lv_event_get_user_data(event));
    view->_controller.emergencyOff(GetHAL()->millis());
    view->refreshAll(GetHAL()->millis());
}

void PpsView::onOutputSwitch(lv_event_t* event)
{
    auto view = static_cast<PpsView*>(lv_event_get_user_data(event));
    if (view->_syncingControls) {
        return;
    }
    bool enabled = lv_obj_has_state(view->_outputSwitch, LV_STATE_CHECKED);
    audio::play_next_tone_progression();
    view->_controller.setOutputEnabled(enabled, GetHAL()->millis());
    view->refreshAll(GetHAL()->millis());
}

void PpsView::onSliderChanged(lv_event_t* event)
{
    auto view = static_cast<PpsView*>(lv_event_get_user_data(event));
    if (view->_syncingControls) {
        return;
    }

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (target == view->_voltageSlider) {
        view->_controller.setPendingVoltage(static_cast<float>(lv_slider_get_value(view->_voltageSlider)) / 10.0f);
    } else if (target == view->_currentSlider) {
        view->_controller.setPendingCurrent(static_cast<float>(lv_slider_get_value(view->_currentSlider)) / 100.0f);
    }
    view->refreshPendingLabels();
}

void PpsView::onPreset(lv_event_t* event)
{
    auto view = static_cast<PpsView*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));

    if (target == view->_preset33) {
        view->_controller.applyPreset(3.3f, 1.0f);
    } else if (target == view->_preset5) {
        view->_controller.applyPreset(5.0f, 2.0f);
    } else if (target == view->_preset9) {
        view->_controller.applyPreset(9.0f, 2.0f);
    } else if (target == view->_preset12) {
        view->_controller.applyPreset(12.0f, 2.0f);
    } else if (target == view->_presetCustom) {
        const auto& state = view->_controller.state();
        view->_controller.applyPreset(state.customVoltage, state.customCurrent);
    }

    audio::play_next_tone_progression();
    view->refreshAll(GetHAL()->millis());
}
