#include <cassert>

#include "apps/app_hardware_i2c/i2c_scan_layout.h"
#include "apps/ui/ui_layout.h"

static void test_i2c_scan_regions_fit_visible_content()
{
    const auto layout = app_hardware_i2c::makeI2cScanLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.resultsPanel));
}

static void test_i2c_scan_action_buttons_fit_content()
{
    const auto layout = app_hardware_i2c::makeI2cScanLayout();

    for (int i = 0; i < app_hardware_i2c::kI2cScanActionCount; ++i) {
        const auto button = app_hardware_i2c::i2cScanActionRect(layout, i);
        assert(app_ui::containsLocal(layout.content, button));
        assert(button.w >= 200);
        assert(button.h >= 56);
    }
}

static void test_i2c_scan_results_fit_panel()
{
    const auto layout = app_hardware_i2c::makeI2cScanLayout();

    assert(app_ui::containsLocal(layout.resultsPanel, layout.resultsTitle));
    assert(app_ui::containsLocal(layout.resultsPanel, layout.resultsText));
    assert(layout.resultsText.w >= 780);
    assert(layout.resultsText.h >= 280);
}

static void test_i2c_scan_vertical_spacing_is_stable()
{
    const auto layout = app_hardware_i2c::makeI2cScanLayout();
    const auto firstButton = app_hardware_i2c::i2cScanActionRect(layout, 0);

    assert(layout.description.y + layout.description.h + layout.sectionGap <= firstButton.y);
    assert(firstButton.y + firstButton.h + layout.sectionGap <= layout.resultsPanel.y);
}

int main()
{
    test_i2c_scan_regions_fit_visible_content();
    test_i2c_scan_action_buttons_fit_content();
    test_i2c_scan_results_fit_panel();
    test_i2c_scan_vertical_spacing_is_stable();
    return 0;
}
