#include <apps/app_settings_power/power_settings_layout.h>
#include <cassert>
#include <vector>

static bool overlap(const app_ui::Rect& a, const app_ui::Rect& b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

int main()
{
    const auto l = app_settings_power::makePowerSettingsLayout();
    std::vector<app_ui::Rect> rects = {l.batteryCard, l.statusTitle, l.statusText, l.detailText, l.wakeButton,
        l.touchCaption, l.motionCaption, l.dangerCaption, l.intervalLabel, l.intervalSlider};
    for (int i = 0; i < app_settings_power::kPowerSettingsActionCount; ++i) {
        const auto button = app_settings_power::powerSettingsActionRect(l, i);
        assert(button.w >= 180 && button.h >= 44);
        rects.push_back(button);
        rects.push_back(app_settings_power::powerSettingsTitleRect(i));
    }
    for (size_t i = 0; i < rects.size(); ++i) {
        assert(app_ui::containsLocal(l.content, rects[i]));
        for (size_t j = i + 1; j < rects.size(); ++j) assert(!overlap(rects[i], rects[j]));
    }
}
