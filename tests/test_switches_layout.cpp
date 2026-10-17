#include <cassert>

#include "apps/app_hardware_switches/switches_layout.h"
#include "apps/ui/ui_layout.h"

static void test_switches_header_and_status_fit_visible_content()
{
    const auto layout = app_hardware_switches::makeSwitchesLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.interfaceStatus));
}

static void test_switch_cards_fit_content_width()
{
    const auto layout = app_hardware_switches::makeSwitchesLayout();

    for (int i = 0; i < app_hardware_switches::kSwitchControlCount; ++i) {
        const auto card = app_hardware_switches::switchCardRect(layout, i);
        assert(card.x >= 0);
        assert(card.x + card.w <= layout.content.w);
        assert(card.w >= 360);
        assert(card.h >= 72);
    }
}

static void test_switch_cards_do_not_overlap_status_panel()
{
    const auto layout = app_hardware_switches::makeSwitchesLayout();

    const auto lastCard = app_hardware_switches::switchCardRect(layout, app_hardware_switches::kSwitchControlCount - 1);
    assert(lastCard.y + lastCard.h + layout.sectionGap <= layout.interfaceStatus.y);
}

static void test_switch_controls_fit_inside_cards()
{
    const auto layout = app_hardware_switches::makeSwitchesLayout();

    for (int i = 0; i < app_hardware_switches::kSwitchControlCount; ++i) {
        const auto card = app_hardware_switches::switchCardRect(layout, i);
        assert(app_ui::containsLocal(card, layout.switchControl));
    }
}

static void test_all_switches_fit_without_scrolling()
{
    const auto layout = app_hardware_switches::makeSwitchesLayout();

    assert(layout.scrollContentHeight <= layout.content.h);
}

int main()
{
    test_switches_header_and_status_fit_visible_content();
    test_switch_cards_fit_content_width();
    test_switch_cards_do_not_overlap_status_panel();
    test_switch_controls_fit_inside_cards();
    test_all_switches_fit_without_scrolling();
    return 0;
}
