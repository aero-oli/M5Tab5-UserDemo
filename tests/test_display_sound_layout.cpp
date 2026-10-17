#include <cassert>

#include "apps/app_settings_display_sound/display_sound_layout.h"
#include "apps/ui/ui_layout.h"

static void test_display_sound_regions_fit_visible_content()
{
    const auto layout = app_settings_display_sound::makeDisplaySoundLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.brightnessCard));
    assert(app_ui::containsLocal(layout.content, layout.volumeCard));
    assert(app_ui::containsLocal(layout.content, layout.infoPanel));
}

static void test_display_sound_actions_fit_visible_content()
{
    const auto layout = app_settings_display_sound::makeDisplaySoundLayout();

    for (int i = 0; i < app_settings_display_sound::kDisplaySoundActionCount; ++i) {
        const auto button = app_settings_display_sound::displaySoundActionRect(layout, i);
        assert(app_ui::containsLocal(layout.content, button));
        assert(button.h >= 56);
    }
}

static void test_display_sound_columns_are_stable()
{
    const auto layout = app_settings_display_sound::makeDisplaySoundLayout();

    assert(layout.brightnessCard.x + layout.brightnessCard.w + layout.columnGap <= layout.volumeCard.x);
    assert(app_settings_display_sound::displaySoundActionRect(layout, 1).x +
               app_settings_display_sound::displaySoundActionRect(layout, 1).w + layout.columnGap <=
           app_settings_display_sound::displaySoundActionRect(layout, 2).x);
}

static void test_display_sound_info_panel_fits_text()
{
    const auto layout = app_settings_display_sound::makeDisplaySoundLayout();

    assert(app_ui::containsLocal(layout.infoPanel, layout.infoText));
    assert(layout.infoText.w >= 700);
    assert(layout.infoText.h >= 28);
}

int main()
{
    test_display_sound_regions_fit_visible_content();
    test_display_sound_actions_fit_visible_content();
    test_display_sound_columns_are_stable();
    test_display_sound_info_panel_fits_text();
    return 0;
}
