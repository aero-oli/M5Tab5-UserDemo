#pragma once
#include <apps/ui/ui_layout.h>

namespace app_settings_power {
inline constexpr int kPowerSettingsActionCount = 4;
inline constexpr int kPowerOffActionIndex = 3;
struct PowerSettingsLayout {
    app_ui::Rect content{0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
    app_ui::Rect batteryCard{800, 0, 432, 150};
    app_ui::Rect statusTitle{800, 192, 432, 28};
    app_ui::Rect statusText{800, 224, 432, 28};
    app_ui::Rect detailText{800, 264, 432, 96};
    app_ui::Rect touchCaption{0, 40, 500, 28};
    app_ui::Rect motionCaption{0, 168, 500, 28};
    app_ui::Rect dangerCaption{0, 504, 500, 28};
    app_ui::Rect wakeButton{800, 392, 220, 56};
    app_ui::Rect intervalLabel{0, 318, 500, 28};
    app_ui::Rect intervalSlider{18, 374, 464, 24};
};
inline PowerSettingsLayout makePowerSettingsLayout() { return {}; }
inline app_ui::Rect powerSettingsActionRect(const PowerSettingsLayout&, int index)
{
    if (index < 0 || index >= kPowerSettingsActionCount) return {};
    return {560, index == 3 ? 480 : 24 + index * 128, 200, 56};
}
inline app_ui::Rect powerSettingsTitleRect(int index)
{
    return {0, index == 3 ? 456 : index * 128, 500, 28};
}
}
