#include <cassert>

#include "apps/app_settings_updates/updates_layout.h"
#include "apps/ui/ui_layout.h"

static void test_updates_top_level_controls_fit_safe_content()
{
    const auto layout = app_settings_updates::makeUpdatesLayout();

    assert(app_ui::containsLocal(layout.content, layout.firmwareCard));
    assert(app_ui::containsLocal(layout.content, layout.stateCard));
    assert(app_ui::containsLocal(layout.content, layout.otaPanel));
    assert(app_ui::containsLocal(layout.content, layout.installButton));
    assert(app_ui::containsLocal(layout.content, layout.cancelButton));
    assert(app_ui::containsLocal(layout.content, layout.rebootButton));
    assert(app_ui::containsLocal(layout.content, layout.progressBar));
    assert(app_ui::containsLocal(layout.content, layout.detailText));
}

static void test_updates_panel_children_fit_local_bounds()
{
    const auto layout = app_settings_updates::makeUpdatesLayout();

    assert(app_ui::containsLocal(layout.otaPanel, layout.urlInput));
    assert(layout.urlInput.w >= 800);
    assert(layout.urlInput.h >= 48);
}

static void test_updates_action_buttons_are_separated()
{
    const auto layout = app_settings_updates::makeUpdatesLayout();

    assert(layout.installButton.x + layout.installButton.w < layout.cancelButton.x);
    assert(layout.cancelButton.x + layout.cancelButton.w < layout.rebootButton.x);
}

int main()
{
    test_updates_top_level_controls_fit_safe_content();
    test_updates_panel_children_fit_local_bounds();
    test_updates_action_buttons_are_separated();
    return 0;
}
