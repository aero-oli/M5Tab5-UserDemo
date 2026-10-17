#include <cassert>

#include "apps/app_media_audio/audio_tool_layout.h"
#include "apps/ui/ui_layout.h"

static void test_audio_tool_regions_fit_visible_content()
{
    const auto layout = app_media_audio::makeAudioToolLayout(true);

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.stateCard));
    assert(app_ui::containsLocal(layout.content, layout.startButton));
    assert(app_ui::containsLocal(layout.content, layout.statusPanel));
}

static void test_audio_tool_optional_stop_button_fits_when_present()
{
    const auto layout = app_media_audio::makeAudioToolLayout(true);

    assert(layout.hasStopButton);
    assert(app_ui::containsLocal(layout.content, layout.stopButton));
    assert(layout.startButton.x + layout.startButton.w + layout.actionGap <= layout.stopButton.x);
}

static void test_audio_tool_stop_button_absent_when_not_needed()
{
    const auto layout = app_media_audio::makeAudioToolLayout(false);

    assert(!layout.hasStopButton);
    assert(layout.stopButton.w == 0);
    assert(layout.stopButton.h == 0);
}

static void test_audio_tool_status_content_fits_panel()
{
    const auto layout = app_media_audio::makeAudioToolLayout(true);

    assert(app_ui::containsLocal(layout.statusPanel, layout.statusTitle));
    assert(app_ui::containsLocal(layout.statusPanel, layout.statusDetail));
    assert(layout.statusDetail.w >= 700);
    assert(layout.statusDetail.h >= 70);
}

static void test_audio_tool_vertical_flow_is_stable()
{
    const auto layout = app_media_audio::makeAudioToolLayout(true);

    assert(layout.description.y + layout.description.h + layout.sectionGap <= layout.stateCard.y);
    assert(layout.stateCard.y + layout.stateCard.h + layout.sectionGap <= layout.startButton.y);
    assert(layout.startButton.y + layout.startButton.h + layout.sectionGap <= layout.statusPanel.y);
}

int main()
{
    test_audio_tool_regions_fit_visible_content();
    test_audio_tool_optional_stop_button_fits_when_present();
    test_audio_tool_stop_button_absent_when_not_needed();
    test_audio_tool_status_content_fits_panel();
    test_audio_tool_vertical_flow_is_stable();
    return 0;
}
