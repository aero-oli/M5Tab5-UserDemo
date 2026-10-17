#include <cassert>

#include "apps/app_hardware_rtc/rtc_layout.h"
#include "apps/ui/ui_layout.h"

static void test_rtc_regions_fit_visible_content()
{
    const auto layout = app_hardware_rtc::makeRtcLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.timeCard));
    assert(app_ui::containsLocal(layout.content, layout.dateCard));
    assert(app_ui::containsLocal(layout.content, layout.sourcePanel));
}

static void test_rtc_metric_cards_have_stable_spacing()
{
    const auto layout = app_hardware_rtc::makeRtcLayout();

    assert(layout.timeCard.x + layout.timeCard.w + layout.cardGap <= layout.dateCard.x);
    assert(layout.timeCard.w >= 360);
    assert(layout.dateCard.w >= 360);
}

static void test_rtc_source_content_fits_panel()
{
    const auto layout = app_hardware_rtc::makeRtcLayout();

    assert(app_ui::containsLocal(layout.sourcePanel, layout.sourceTitle));
    assert(app_ui::containsLocal(layout.sourcePanel, layout.sourceDetail));
    assert(layout.sourceDetail.w >= 680);
    assert(layout.sourceDetail.h >= 80);
}

int main()
{
    test_rtc_regions_fit_visible_content();
    test_rtc_metric_cards_have_stable_spacing();
    test_rtc_source_content_fits_panel();
    return 0;
}
