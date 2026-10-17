#include <cassert>

#include "apps/app_settings/about_layout.h"
#include "apps/ui/ui_layout.h"

static void test_about_regions_fit_visible_content()
{
    const auto layout = app_settings::makeAboutLayout();

    assert(app_ui::containsLocal(layout.content, layout.deviceCard));
    assert(app_ui::containsLocal(layout.content, layout.displayCard));
    assert(app_ui::containsLocal(layout.content, layout.infoPanel));
}

static void test_about_cards_do_not_overlap()
{
    const auto layout = app_settings::makeAboutLayout();

    assert(layout.deviceCard.x + layout.deviceCard.w + layout.cardGap <= layout.displayCard.x);
}

static void test_about_panel_text_fits_panel()
{
    const auto layout = app_settings::makeAboutLayout();

    assert(app_ui::containsLocal(layout.infoPanel, layout.registryText));
    assert(app_ui::containsLocal(layout.infoPanel, layout.categoryText));
    assert(layout.registryText.w >= 860);
    assert(layout.categoryText.w >= 860);
}

int main()
{
    test_about_regions_fit_visible_content();
    test_about_cards_do_not_overlap();
    test_about_panel_text_fits_panel();
    return 0;
}
