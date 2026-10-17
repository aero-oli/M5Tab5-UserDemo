#include <cassert>

#include "apps/app_hardware_imu/imu_layout.h"
#include "apps/ui/ui_layout.h"

static void test_imu_header_and_detail_fit_visible_content()
{
    const auto layout = app_hardware_imu::makeImuLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.detailText));
}

static void test_imu_metric_cards_fit_visible_content()
{
    const auto layout = app_hardware_imu::makeImuLayout();

    for (int i = 0; i < app_hardware_imu::kImuMetricCount; ++i) {
        const auto card = app_hardware_imu::imuMetricRect(layout, i);
        assert(app_ui::containsLocal(layout.content, card));
        assert(card.w >= 280);
        assert(card.h >= 132);
    }
}

static void test_imu_metric_grid_spacing_is_stable()
{
    const auto layout = app_hardware_imu::makeImuLayout();

    for (int i = 1; i < app_hardware_imu::kImuMetricCount; ++i) {
        const auto previous = app_hardware_imu::imuMetricRect(layout, i - 1);
        const auto current = app_hardware_imu::imuMetricRect(layout, i);
        if (i % layout.cols == 0) {
            assert(previous.y + previous.h + layout.rowGap <= current.y);
        } else {
            assert(previous.x + previous.w + layout.colGap <= current.x);
        }
    }
}

int main()
{
    test_imu_header_and_detail_fit_visible_content();
    test_imu_metric_cards_fit_visible_content();
    test_imu_metric_grid_spacing_is_stable();
    return 0;
}
