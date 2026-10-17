#include <cassert>

#include "apps/app_hardware_power_monitor/power_monitor_layout.h"
#include "apps/ui/ui_layout.h"

static void test_power_monitor_regions_fit_visible_content()
{
    const auto layout = app_hardware_power_monitor::makePowerMonitorLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.readoutPanel));
}

static void test_power_monitor_metric_cards_fit_visible_content()
{
    const auto layout = app_hardware_power_monitor::makePowerMonitorLayout();

    for (int i = 0; i < app_hardware_power_monitor::kPowerMonitorMetricCount; ++i) {
        const auto card = app_hardware_power_monitor::powerMonitorMetricRect(layout, i);
        assert(app_ui::containsLocal(layout.content, card));
        assert(card.w >= 280);
        assert(card.h >= 132);
    }
}

static void test_power_monitor_metric_grid_spacing_is_stable()
{
    const auto layout = app_hardware_power_monitor::makePowerMonitorLayout();

    for (int i = 1; i < app_hardware_power_monitor::kPowerMonitorMetricCount; ++i) {
        const auto previous = app_hardware_power_monitor::powerMonitorMetricRect(layout, i - 1);
        const auto current  = app_hardware_power_monitor::powerMonitorMetricRect(layout, i);
        assert(previous.x + previous.w + layout.colGap <= current.x);
    }
}

static void test_power_monitor_readout_content_fits_panel()
{
    const auto layout = app_hardware_power_monitor::makePowerMonitorLayout();

    assert(app_ui::containsLocal(layout.readoutPanel, layout.readoutTitle));
    assert(app_ui::containsLocal(layout.readoutPanel, layout.readoutDetail));
    assert(layout.readoutDetail.w >= 1100);
    assert(layout.readoutDetail.h >= 100);
}

int main()
{
    test_power_monitor_regions_fit_visible_content();
    test_power_monitor_metric_cards_fit_visible_content();
    test_power_monitor_metric_grid_spacing_is_stable();
    test_power_monitor_readout_content_fits_panel();
    return 0;
}
