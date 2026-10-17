#include <cassert>

#include "apps/app_hardware_sd/sd_card_layout.h"
#include "apps/ui/ui_layout.h"

static void test_sd_card_regions_fit_visible_content()
{
    const auto layout = app_hardware_sd::makeSdCardLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.scanButton));
    assert(app_ui::containsLocal(layout.content, layout.resultsPanel));
}

static void test_sd_card_results_fit_panel()
{
    const auto layout = app_hardware_sd::makeSdCardLayout();

    assert(app_ui::containsLocal(layout.resultsPanel, layout.resultsTitle));
    assert(app_ui::containsLocal(layout.resultsPanel, layout.resultsText));
    assert(layout.resultsText.w >= 780);
    assert(layout.resultsText.h >= 280);
}

static void test_sd_card_vertical_spacing_is_stable()
{
    const auto layout = app_hardware_sd::makeSdCardLayout();

    assert(layout.description.y + layout.description.h + layout.sectionGap <= layout.scanButton.y);
    assert(layout.scanButton.y + layout.scanButton.h + layout.sectionGap <= layout.resultsPanel.y);
}

int main()
{
    test_sd_card_regions_fit_visible_content();
    test_sd_card_results_fit_panel();
    test_sd_card_vertical_spacing_is_stable();
    return 0;
}
