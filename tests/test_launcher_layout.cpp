#include <cassert>
#include <array>
#include "apps/app_launcher/view/launcher_layout.h"
#include "apps/ui/ui_layout.h"

static bool overlaps(const app_ui::Rect& a, const app_ui::Rect& b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
static void test_regions_and_header_fit_without_overlap()
{
    const auto layout = launcher_view::makeLauncherLayout();
    assert(app_ui::contains(layout.screen, layout.topBar));
    assert(app_ui::contains(layout.safe, layout.categoryRail));
    assert(app_ui::contains(layout.safe, layout.tileArea));
    assert(layout.categoryRail.x + layout.categoryRail.w + layout.columnGap == layout.tileArea.x);
    assert(layout.tileArea.x + layout.tileArea.w == layout.safe.x + layout.safe.w);
    const std::array<app_ui::Rect,3> header = {layout.headerTitle,layout.headerStatus,layout.headerTime};
    for (size_t i=0;i<header.size();++i) {
        assert(app_ui::containsLocal(layout.topBar,header[i]));
        for (size_t j=i+1;j<header.size();++j) assert(!overlaps(header[i],header[j]));
    }
    assert(layout.headerStatus.w >= 600); // Full PPS off-failure warning has a dedicated region.
}
static void test_home_tiles_and_their_text_regions_do_not_overlap()
{
    const auto layout = launcher_view::makeLauncherLayout();
    assert(layout.tileColumns == 3 && layout.tileW >= 300);
    const std::array<app_ui::Rect,4> content = {layout.tileIcon,layout.tileTitle,layout.tileDescription,layout.tileStatus};
    for (int i=0;i<6;++i) {
        const auto tile = launcher_view::launcherTileRect(layout,i);
        assert(app_ui::containsLocal(layout.tileArea,tile));
        for (size_t a=0;a<content.size();++a) {
            assert(app_ui::containsLocal(tile,content[a]));
            for (size_t b=a+1;b<content.size();++b) assert(!overlaps(content[a],content[b]));
        }
        for (int j=i+1;j<6;++j) assert(!overlaps(tile,launcher_view::launcherTileRect(layout,j)));
    }
    assert(layout.tileTitle.w >= 200);
    assert(layout.tileDescription.h >= 40);
    assert(layout.tileDescription.y + layout.tileDescription.h < layout.tileStatus.y);
}
static void test_categories_and_scrollable_extra_rows()
{
    const auto layout = launcher_view::makeLauncherLayout();
    for (int i=0;i<5;++i)
        assert(app_ui::containsLocal(layout.categoryRail,launcher_view::launcherCategoryButtonRect(layout,i)));
    // Hardware categories can have more entries than Home; their grid stays in
    // the scrollable tile column rather than crowding siblings or the category rail.
    const auto extra = launcher_view::launcherTileRect(layout,8);
    assert(extra.x >= 0 && extra.x + extra.w <= layout.tileArea.w);
    assert(extra.y > launcher_view::launcherTileRect(layout,5).y);
}
int main()
{
    test_regions_and_header_fit_without_overlap();
    test_home_tiles_and_their_text_regions_do_not_overlap();
    test_categories_and_scrollable_extra_rows();
}
