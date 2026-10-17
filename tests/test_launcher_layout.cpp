#include <cassert>

#include "apps/app_launcher/view/launcher_layout.h"
#include "apps/ui/ui_layout.h"

static void test_launcher_top_level_regions_fit_screen_and_safe_area()
{
    const auto screen = app_ui::screenRect();
    const auto safe = app_ui::safeContentRect();
    const auto layout = launcher_view::makeLauncherLayout();

    assert(app_ui::contains(screen, layout.topBar));
    assert(app_ui::contains(safe, layout.categoryRail));
    assert(app_ui::contains(safe, layout.tileArea));
    assert(app_ui::contains(safe, layout.summaryArea));
}

static void test_launcher_columns_do_not_overlap()
{
    const auto layout = launcher_view::makeLauncherLayout();

    assert(layout.categoryRail.x + layout.categoryRail.w + layout.columnGap == layout.tileArea.x);
    assert(layout.tileArea.x + layout.tileArea.w + layout.columnGap == layout.summaryArea.x);
    assert(layout.summaryArea.x + layout.summaryArea.w == app_ui::safeContentRect().x + app_ui::safeContentRect().w);
}

static void test_launcher_tiles_fit_tile_area_local_bounds()
{
    const auto layout = launcher_view::makeLauncherLayout();

    for (int i = 0; i < 6; ++i) {
        const auto tile = launcher_view::launcherTileRect(layout, i);
        assert(app_ui::containsLocal(layout.tileArea, tile));
        assert(app_ui::containsLocal(tile, layout.tileIcon));
        assert(app_ui::containsLocal(tile, layout.tileTitle));
        assert(app_ui::containsLocal(tile, layout.tileDescription));
        assert(app_ui::containsLocal(tile, layout.tileStatus));
    }
}

static void test_launcher_category_buttons_and_summary_rows_fit()
{
    const auto layout = launcher_view::makeLauncherLayout();

    for (int i = 0; i < 5; ++i) {
        assert(app_ui::containsLocal(layout.categoryRail, launcher_view::launcherCategoryButtonRect(layout, i)));
    }
    for (int i = 0; i < 10; ++i) {
        assert(app_ui::containsLocal(layout.summaryArea, launcher_view::launcherSummaryLabelRect(layout, i)));
        assert(app_ui::containsLocal(layout.summaryArea, launcher_view::launcherSummaryValueRect(layout, i)));
    }
}

int main()
{
    test_launcher_top_level_regions_fit_screen_and_safe_area();
    test_launcher_columns_do_not_overlap();
    test_launcher_tiles_fit_tile_area_local_bounds();
    test_launcher_category_buttons_and_summary_rows_fit();
    return 0;
}
