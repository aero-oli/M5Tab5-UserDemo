#include <cassert>

#include "apps/app_settings_network/network_layout.h"
#include "apps/ui/ui_layout.h"

static void test_network_top_level_controls_fit_safe_content()
{
    const auto layout = app_settings_network::makeNetworkLayout();

    assert(app_ui::containsLocal(layout.content, layout.wifiCard));
    assert(app_ui::containsLocal(layout.content, layout.addressCard));
    assert(app_ui::containsLocal(layout.content, layout.statusText));
    assert(app_ui::containsLocal(layout.content, layout.stationPanel));
    assert(app_ui::containsLocal(layout.content, layout.apPanel));
    assert(app_ui::containsLocal(layout.content, layout.scanButton));
    assert(app_ui::containsLocal(layout.content, layout.connectButton));
    assert(app_ui::containsLocal(layout.content, layout.disconnectButton));
    assert(app_ui::containsLocal(layout.content, layout.forgetButton));
    assert(app_ui::containsLocal(layout.content, layout.startApButton));
    assert(app_ui::containsLocal(layout.content, layout.stopApButton));
    assert(app_ui::containsLocal(layout.content, layout.detailText));
    assert(app_ui::containsLocal(layout.content, layout.networksPanel));
    assert(layout.networksPanel.x + layout.networksPanel.w < layout.stationPanel.x);
}

static void test_network_panel_children_fit_local_bounds()
{
    const auto layout = app_settings_network::makeNetworkLayout();

    assert(app_ui::containsLocal(layout.stationPanel, layout.ssidInput));
    assert(app_ui::containsLocal(layout.stationPanel, layout.passwordInput));
    assert(app_ui::containsLocal(layout.apPanel, layout.apDescription));
    assert(app_ui::containsLocal(layout.apPanel, layout.extAntennaSwitch));
    assert(layout.apDescription.w <= layout.apPanel.w - 32);
    assert(app_ui::containsLocal(layout.networksPanel, layout.networksList));
    assert(app_ui::containsLocal(layout.apPanel, layout.antennaLabel));
    assert(layout.antennaLabel.x + layout.antennaLabel.w < layout.extAntennaSwitch.x);
}

static void test_network_action_row_keeps_buttons_separated()
{
    const auto layout = app_settings_network::makeNetworkLayout();

    assert(layout.scanButton.x + layout.scanButton.w < layout.connectButton.x);
    assert(layout.connectButton.x + layout.connectButton.w < layout.disconnectButton.x);
    assert(layout.disconnectButton.x + layout.disconnectButton.w < layout.forgetButton.x);
    assert(layout.startApButton.x + layout.startApButton.w < layout.stopApButton.x);
}

int main()
{
    test_network_top_level_controls_fit_safe_content();
    test_network_panel_children_fit_local_bounds();
    test_network_action_row_keeps_buttons_separated();
    return 0;
}
