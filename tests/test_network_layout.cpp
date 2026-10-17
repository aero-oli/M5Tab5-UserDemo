#include <cassert>
#include <algorithm>

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

static void test_populated_list_rows_stay_inside_viewport_width_and_scroll_to_last()
{
    const auto layout = app_settings_network::makeNetworkLayout();
    assert(app_settings_network::networkRowsHeight(layout, 0) == 0);
    assert(app_settings_network::networkRowsHeight(layout, 3) <= layout.networksList.h);
    for (size_t count : {size_t(40), size_t(43), size_t(64)}) {
        const int height = app_settings_network::networkRowsHeight(layout, count);
        assert(height > layout.networksList.h);
        for (size_t index = 0; index < count; ++index) {
            const auto row = app_settings_network::networkRowRect(layout, index);
            assert(row.x >= 0 && row.x + row.w <= layout.networksList.w - 12);
            assert(row.h > 0 && row.h <= layout.networksList.h);
            if (index) {
                const auto previous = app_settings_network::networkRowRect(layout, index - 1);
                assert(previous.y + previous.h < row.y);
            }
            // A bounded viewport can fully expose every row with vertical scrolling.
            const int scroll = std::max(0, row.y + row.h - layout.networksList.h);
            assert(scroll <= height - layout.networksList.h);
            assert(row.y - scroll >= 0 && row.y + row.h - scroll <= layout.networksList.h);
        }
    }
    // The viewport is below Scan and left of all station/AP fixed actions.
    const int listTop = layout.networksPanel.y + 16 + layout.networksList.y;
    assert(layout.scanButton.y + layout.scanButton.h < listTop);
    assert(layout.networksPanel.x + layout.networksPanel.w < layout.connectButton.x);
}

int main()
{
    test_network_top_level_controls_fit_safe_content();
    test_network_panel_children_fit_local_bounds();
    test_network_action_row_keeps_buttons_separated();
    test_populated_list_rows_stay_inside_viewport_width_and_scroll_to_last();
    return 0;
}
