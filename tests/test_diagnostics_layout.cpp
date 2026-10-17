#include <cassert>

#include "apps/app_diagnostics/diagnostics_layout.h"
#include "apps/ui/ui_layout.h"

static void test_diagnostics_regions_fit_visible_content()
{
    const auto layout = app_diagnostics::makeDiagnosticsLayout();

    assert(app_ui::containsLocal(layout.content, layout.description));
    assert(app_ui::containsLocal(layout.content, layout.detailPanel));
}

static void test_diagnostics_metric_cards_fit_visible_content()
{
    const auto layout = app_diagnostics::makeDiagnosticsLayout();

    for (int i = 0; i < app_diagnostics::kDiagnosticsMetricCount; ++i) {
        const auto card = app_diagnostics::diagnosticsMetricRect(layout, i);
        assert(app_ui::containsLocal(layout.content, card));
        assert(card.w >= 280);
        assert(card.h >= 132);
    }
}

static void test_diagnostics_metric_grid_spacing_is_stable()
{
    const auto layout = app_diagnostics::makeDiagnosticsLayout();

    for (int i = 1; i < app_diagnostics::kDiagnosticsMetricCount; ++i) {
        const auto previous = app_diagnostics::diagnosticsMetricRect(layout, i - 1);
        const auto current  = app_diagnostics::diagnosticsMetricRect(layout, i);
        assert(previous.x + previous.w + layout.colGap <= current.x);
    }
}

static void test_diagnostics_detail_content_fits_panel()
{
    const auto layout = app_diagnostics::makeDiagnosticsLayout();

    assert(app_ui::containsLocal(layout.detailPanel, layout.detailHint));
    assert(app_ui::containsLocal(layout.detailPanel, layout.detailText));
    assert(layout.detailText.w >= 1100);
    assert(layout.detailText.h >= 100);
}

int main()
{
    test_diagnostics_regions_fit_visible_content();
    test_diagnostics_metric_cards_fit_visible_content();
    test_diagnostics_metric_grid_spacing_is_stable();
    test_diagnostics_detail_content_fits_panel();
    return 0;
}
