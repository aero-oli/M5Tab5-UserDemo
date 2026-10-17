/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <apps/ui/ui_layout.h>

namespace app_media_camera {

struct CameraLayout {
    app_ui::Rect content;
    app_ui::Rect previewPanel;
    app_ui::Rect cameraCanvas;
    app_ui::Rect controlPanel;
    app_ui::Rect controlTitle;
    app_ui::Rect statusText;
    app_ui::Rect startButton;
    app_ui::Rect stopButton;
    app_ui::Rect hint;
    app_ui::Rect errorText;
    int columnGap = 28;
    int controlGap = 20;
    int sectionGap = 28;
};

inline app_ui::Rect cameraContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}

inline CameraLayout makeCameraLayout()
{
    CameraLayout layout;
    layout.content = cameraContentRect();
    layout.previewPanel = {0, 0, 900, 540};
    layout.cameraCanvas = {20, 28, 860, 484};
    layout.controlPanel = {layout.previewPanel.x + layout.previewPanel.w + layout.columnGap, 0, 280, 340};
    layout.controlTitle = {0, 0, 240, 34};
    layout.statusText = {0, 52, 240, 68};
    layout.startButton = {0, 132, 220, 64};
    layout.stopButton = {0, layout.startButton.y + layout.startButton.h + layout.controlGap, 220, 64};
    layout.hint = {layout.controlPanel.x, layout.controlPanel.y + layout.controlPanel.h + layout.sectionGap, 260, 112};
    layout.errorText = {0, 560, layout.content.w, 40};
    return layout;
}

}  // namespace app_media_camera
