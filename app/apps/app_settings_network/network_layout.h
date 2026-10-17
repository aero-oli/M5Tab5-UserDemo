/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_settings_network {

struct NetworkLayout {
    app_ui::Rect content;
    app_ui::Rect wifiCard;
    app_ui::Rect addressCard;
    app_ui::Rect statusText;
    app_ui::Rect stationPanel;
    app_ui::Rect ssidInput;
    app_ui::Rect passwordInput;
    app_ui::Rect scanButton;
    app_ui::Rect connectButton;
    app_ui::Rect disconnectButton;
    app_ui::Rect forgetButton;
    app_ui::Rect apPanel;
    app_ui::Rect apDescription;
    app_ui::Rect extAntennaSwitch;
    app_ui::Rect startApButton;
    app_ui::Rect stopApButton;
    app_ui::Rect detailText;
    app_ui::Rect networksPanel;
    app_ui::Rect networksList;
    app_ui::Rect antennaLabel;
};

inline NetworkLayout makeNetworkLayout()
{
    return {
        {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h},
        {0, 0, 360, 108},
        {384, 0, 360, 108},
        {768, 0, 464, 108},
        {624, 128, 608, 216},
        {0, 44, 276, 52},
        {292, 44, 276, 52},
        {424, 144, 156, 48},
        {641, 252, 160, 56},
        {817, 252, 160, 56},
        {993, 252, 160, 56},
        {624, 364, 608, 180},
        {0, 36, 304, 48},
        {490, 36, 72, 38},
        {641, 474, 240, 52},
        {897, 474, 240, 52},
        {0, 560, 1232, 40},
        {0, 128, 600, 416},
        {0, 64, 566, 318},
        {324, 42, 156, 26},
    };
}

}  // namespace app_settings_network
