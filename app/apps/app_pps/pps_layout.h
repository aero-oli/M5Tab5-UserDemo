/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_pps {

struct PpsLayout {
    app_ui::Rect content;
    app_ui::Rect statusStrip;
    app_ui::Rect telemetryPanel;
    app_ui::Rect diagnosticsPanel;
    app_ui::Rect controlsPanel;
    app_ui::Rect errorLabel;
    app_ui::Rect outputSwitch;
    app_ui::Rect emergencyOffButton;
    app_ui::Rect voltageSlider;
    app_ui::Rect currentSlider;
    app_ui::Rect voltageInput;
    app_ui::Rect currentInput;
    app_ui::Rect preset33Button;
    app_ui::Rect preset5Button;
    app_ui::Rect preset9Button;
    app_ui::Rect preset12Button;
    app_ui::Rect presetCustomButton;
    app_ui::Rect applyButton;
    app_ui::Rect connectionText;
    app_ui::Rect modeText;
    app_ui::Rect vinText;
    app_ui::Rect temperatureText;
};

inline app_ui::Rect ppsContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline PpsLayout makePpsLayout()
{
    return {
        ppsContentRect(),
        {0, 0, 1232, 54},
        {0, 76, 580, 220},
        {0, 320, 580, 236},
        {604, 76, 628, 480},
        {0, 568, 1232, 32},
        {118, 50, 72, 38},
        {344, 44, 204, 54},
        {0, 166, 548, 18},
        {0, 252, 548, 18},
        {300, 116, 248, 44},
        {300, 202, 248, 44},
        {0, 306, 88, 44},
        {98, 306, 88, 44},
        {196, 306, 88, 44},
        {294, 306, 88, 44},
        {392, 306, 88, 44},
        {0, 382, 548, 56},
        {0, 0, 360, 24},
        {380, 0, 180, 24},
        {570, 0, 160, 24},
        {750, 0, 180, 24},
    };
}

}  // namespace app_pps
