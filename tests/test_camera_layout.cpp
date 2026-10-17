#include <cassert>

#include "apps/app_media_camera/camera_layout.h"
#include "apps/ui/ui_layout.h"
#include "hal/camera_preview.h"

static void test_camera_frame_is_letterboxed_without_cropping()
{
    const auto fit = hal::fitCameraPreview(1280, 720, 860, 484);
    assert(fit.valid());
    assert(fit.scale == 0.625f);
    assert(fit.width == 800 && fit.height == 450);
    assert(fit.x == 30 && fit.y == 17);
    for (const auto& source : {app_ui::Rect{0, 0, 1920, 1080}, app_ui::Rect{0, 0, 720, 1280},
                              app_ui::Rect{0, 0, 640, 480}}) {
        const auto fitted = hal::fitCameraPreview(source.w, source.h, 860, 484);
        assert(fitted.valid());
        assert(app_ui::contains({0, 0, 860, 484}, {fitted.x, fitted.y, fitted.width, fitted.height}));
        assert(std::fabs(fitted.width - source.w * fitted.scale) < 1.0f);
        assert(std::fabs(fitted.height - source.h * fitted.scale) < 1.0f);
    }
    assert(!hal::fitCameraPreview(0, 720, 860, 484).valid());
    assert(!hal::fitCameraPreview(1280, 720, -1, 484).valid());
    assert(!hal::fitCameraPreview(1280, 720, 1, 1).valid());
}

static void test_camera_major_regions_fit_content()
{
    const auto layout = app_media_camera::makeCameraLayout();

    assert(app_ui::containsLocal(layout.content, layout.previewPanel));
    assert(app_ui::containsLocal(layout.content, layout.controlPanel));
    assert(app_ui::containsLocal(layout.content, layout.hint));
    assert(app_ui::containsLocal(layout.content, layout.errorText));
}

static void test_camera_canvas_fits_preview_panel()
{
    const auto layout = app_media_camera::makeCameraLayout();

    assert(app_ui::containsLocal(layout.previewPanel, layout.cameraCanvas));
    assert(layout.cameraCanvas.w >= 800);
    assert(layout.cameraCanvas.h >= 450);
}

static void test_camera_controls_fit_panel()
{
    const auto layout = app_media_camera::makeCameraLayout();

    assert(app_ui::containsLocal(layout.controlPanel, layout.controlTitle));
    assert(app_ui::containsLocal(layout.controlPanel, layout.statusText));
    assert(app_ui::containsLocal(layout.controlPanel, layout.startButton));
    assert(app_ui::containsLocal(layout.controlPanel, layout.stopButton));
    assert(layout.startButton.y + layout.startButton.h + layout.controlGap <= layout.stopButton.y);
}

static void test_camera_regions_do_not_overlap()
{
    const auto layout = app_media_camera::makeCameraLayout();

    assert(layout.previewPanel.x + layout.previewPanel.w + layout.columnGap <= layout.controlPanel.x);
    assert(layout.previewPanel.x + layout.previewPanel.w + layout.columnGap <= layout.hint.x);
    assert(layout.controlPanel.y + layout.controlPanel.h + layout.sectionGap <= layout.hint.y);
    assert(layout.previewPanel.y + layout.previewPanel.h <= layout.errorText.y);
    assert(layout.hint.y + layout.hint.h <= layout.errorText.y);
}

int main()
{
    test_camera_major_regions_fit_content();
    test_camera_canvas_fits_preview_panel();
    test_camera_controls_fit_panel();
    test_camera_regions_do_not_overlap();
    test_camera_frame_is_letterboxed_without_cropping();
    return 0;
}
