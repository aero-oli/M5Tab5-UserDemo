#include <cassert>

#include "apps/ui/app_shell_layout.h"
#include "apps/ui/ui_layout.h"

static void test_shell_regions_fit_screen_and_safe_content()
{
    const auto layout = app_ui::makeAppShellLayout();

    assert(app_ui::contains(app_ui::screenRect(), layout.root));
    assert(app_ui::contains(app_ui::screenRect(), layout.header));
    assert(app_ui::contains(app_ui::screenRect(), layout.content));
    assert(layout.header.x == 0);
    assert(layout.header.y == 0);
    assert(layout.header.h == 72);
    assert(layout.content.x == app_ui::safeContentRect().x);
    assert(layout.content.y == app_ui::safeContentRect().y);
    assert(layout.content.w == app_ui::safeContentRect().w);
    assert(layout.content.h == app_ui::safeContentRect().h);
    assert(layout.header.y + layout.header.h < layout.content.y);
}

static void test_header_controls_are_inside_header()
{
    const auto layout = app_ui::makeAppShellLayout();

    assert(app_ui::containsLocal(layout.header, layout.backButton));
    assert(app_ui::containsLocal(layout.header, layout.title));
    assert(app_ui::containsLocal(layout.header, layout.chipArea));
    assert(layout.backButton.x + layout.backButton.w < layout.title.x);
    assert(layout.title.x + layout.title.w < layout.chipArea.x);
}

static void test_chip_slots_fit_and_clamp_to_header_area()
{
    const auto layout = app_ui::makeAppShellLayout();

    assert(app_ui::maxAppShellChips(layout) == 3);
    for (int i = 0; i < app_ui::maxAppShellChips(layout); ++i) {
        assert(app_ui::containsLocal(layout.chipArea, app_ui::appShellChipRect(layout, i)));
    }
    assert(app_ui::appShellChipRect(layout, app_ui::maxAppShellChips(layout)).w == 0);
}

int main()
{
    test_shell_regions_fit_screen_and_safe_content();
    test_header_controls_are_inside_header();
    test_chip_slots_fit_and_clamp_to_header_area();
    return 0;
}
