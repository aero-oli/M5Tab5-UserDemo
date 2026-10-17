#include <cassert>

#include "apps/app_pps/pps_layout.h"
#include "apps/ui/ui_layout.h"

static void test_pps_top_level_regions_fit_safe_content()
{
    const auto safe = app_pps::ppsContentRect();
    const auto layout = app_pps::makePpsLayout();

    assert(app_ui::contains(safe, layout.statusStrip));
    assert(app_ui::contains(safe, layout.telemetryPanel));
    assert(app_ui::contains(safe, layout.diagnosticsPanel));
    assert(app_ui::contains(safe, layout.controlsPanel));
    assert(app_ui::contains(safe, layout.errorLabel));
}

static void test_pps_controls_fit_the_controls_panel()
{
    const auto layout = app_pps::makePpsLayout();

    assert(app_ui::containsLocal(layout.controlsPanel, layout.outputSwitch));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.emergencyOffButton));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.voltageSlider));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.currentSlider));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.voltageInput));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.currentInput));
    assert(layout.voltageInput.y + layout.voltageInput.h < layout.voltageSlider.y);
    assert(layout.currentInput.y + layout.currentInput.h < layout.currentSlider.y);
    assert(app_ui::containsLocal(layout.controlsPanel, layout.preset33Button));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.preset5Button));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.preset9Button));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.preset12Button));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.presetCustomButton));
    assert(app_ui::containsLocal(layout.controlsPanel, layout.applyButton));
}

static void test_pps_danger_actions_are_always_visible()
{
    const auto layout = app_pps::makePpsLayout();

    assert(layout.emergencyOffButton.w >= 180);
    assert(layout.emergencyOffButton.h >= 48);
    assert(layout.emergencyOffButton.y < layout.applyButton.y);
}

static void test_pps_status_and_actions_have_separate_space()
{
    const auto layout = app_pps::makePpsLayout();
    assert(app_ui::containsLocal(layout.statusStrip, layout.connectionText));
    assert(app_ui::containsLocal(layout.statusStrip, layout.modeText));
    assert(app_ui::containsLocal(layout.statusStrip, layout.vinText));
    assert(app_ui::containsLocal(layout.statusStrip, layout.temperatureText));
    assert(layout.connectionText.x + layout.connectionText.w < layout.modeText.x);
    assert(layout.modeText.x + layout.modeText.w < layout.vinText.x);
    assert(layout.vinText.x + layout.vinText.w < layout.temperatureText.x);
    assert(layout.applyButton.w >= 180);
    assert(layout.applyButton.y >= layout.presetCustomButton.y + layout.presetCustomButton.h + 16);
    assert(layout.errorLabel.y >= layout.controlsPanel.y + layout.controlsPanel.h);
    assert(layout.errorLabel.y >= layout.diagnosticsPanel.y + layout.diagnosticsPanel.h);
}

int main()
{
    test_pps_top_level_regions_fit_safe_content();
    test_pps_controls_fit_the_controls_panel();
    test_pps_danger_actions_are_always_visible();
    test_pps_status_and_actions_have_separate_space();
    return 0;
}
