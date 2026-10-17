#include <cassert>

#include "apps/app_hardware_com_monitor/com_monitor_layout.h"
#include "apps/ui/ui_layout.h"

static void test_com_monitor_regions_fit_visible_content()
{
    const auto layout = app_hardware_com_monitor::makeComMonitorLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.logPanel));
    assert(app_ui::containsLocal(layout.content, layout.sendButton));
    assert(app_ui::containsLocal(layout.content, layout.statusText));
}

static void test_com_monitor_log_content_fits_panel()
{
    const auto layout = app_hardware_com_monitor::makeComMonitorLayout();

    assert(app_ui::containsLocal(layout.logPanel, layout.logTitle));
    assert(app_ui::containsLocal(layout.logPanel, layout.logText));
    assert(layout.logText.w >= 900);
    assert(layout.logText.h >= 320);
}

static void test_com_monitor_bottom_row_is_below_log_and_visible()
{
    const auto layout = app_hardware_com_monitor::makeComMonitorLayout();

    assert(layout.logPanel.y + layout.logPanel.h + layout.sectionGap <= layout.sendButton.y);
    assert(layout.sendButton.y == layout.statusText.y - 12);
    assert(layout.statusText.x > layout.sendButton.x + layout.sendButton.w);
}

int main()
{
    test_com_monitor_regions_fit_visible_content();
    test_com_monitor_log_content_fits_panel();
    test_com_monitor_bottom_row_is_below_log_and_visible();
    return 0;
}
