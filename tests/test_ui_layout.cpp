#include <cassert>
#include <vector>

#include "apps/ui/ui_layout.h"

static void test_safe_content_area_is_fixed_for_tab5()
{
    auto rect = app_ui::safeContentRect();
    assert(rect.x == 24);
    assert(rect.y == 96);
    assert(rect.w == 1232);
    assert(rect.h == 600);
    assert(app_ui::contains(app_ui::screenRect(), rect));
}

static void test_launcher_grid_fits_content_area()
{
    auto content = app_ui::safeContentRect();
    app_ui::Rect tileArea{254, 96, 664, 600};
    auto tiles = app_ui::makeGrid(tileArea, 3, 2, 18, 214, 142);
    assert(tiles.size() == 6);
    for (const auto& tile : tiles) {
        assert(app_ui::contains(content, tile));
    }
}

static void test_dashboard_columns_fit_safe_area()
{
    auto content = app_ui::safeContentRect();
    app_ui::Rect rail{24, 96, 210, 600};
    app_ui::Rect grid{254, 96, 664, 600};
    app_ui::Rect summary{938, 96, 318, 600};
    assert(app_ui::contains(content, rail));
    assert(app_ui::contains(content, grid));
    assert(app_ui::contains(content, summary));
}

static void test_rejects_out_of_bounds_children()
{
    app_ui::Rect parent{24, 96, 1232, 600};
    app_ui::Rect child{1200, 660, 100, 80};
    assert(!app_ui::contains(parent, child));
}

static void test_local_child_bounds_use_parent_dimensions()
{
    app_ui::Rect parent{604, 76, 628, 504};
    app_ui::Rect child{492, 298, 70, 60};
    app_ui::Rect outside{590, 480, 70, 60};
    assert(app_ui::containsLocal(parent, child));
    assert(!app_ui::containsLocal(parent, outside));
}

static void test_shared_columns_support_a_flexible_middle_region()
{
    auto safe    = app_ui::safeContentRect();
    auto columns = app_ui::makeColumns(safe, {210, 0, 318}, 20);

    assert(columns.size() == 3);
    assert(columns[0].x == safe.x);
    assert(columns[0].w == 210);
    assert(columns[1].x == columns[0].x + columns[0].w + 20);
    assert(columns[1].w == 664);
    assert(columns[2].x == columns[1].x + columns[1].w + 20);
    assert(columns[2].w == 318);
    assert(columns[2].x + columns[2].w == safe.x + safe.w);
}

static void test_shared_columns_reject_impossible_widths()
{
    auto columns = app_ui::makeColumns(app_ui::Rect{0, 0, 500, 100}, {300, 300}, 24);

    assert(columns.empty());
}

int main()
{
    test_safe_content_area_is_fixed_for_tab5();
    test_launcher_grid_fits_content_area();
    test_dashboard_columns_fit_safe_area();
    test_rejects_out_of_bounds_children();
    test_local_child_bounds_use_parent_dimensions();
    test_shared_columns_support_a_flexible_middle_region();
    test_shared_columns_reject_impossible_widths();
    return 0;
}
