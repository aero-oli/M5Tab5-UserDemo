/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <algorithm>
#include <cmath>

namespace hal {

struct CameraPreviewTransform {
    float scale = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool valid() const { return scale >= 1.0f / 16.0f && scale < 256.0f && width > 0 && height > 0; }
};

inline CameraPreviewTransform fitCameraPreview(int sourceWidth, int sourceHeight, int width, int height)
{
    if (sourceWidth <= 0 || sourceHeight <= 0 || width <= 0 || height <= 0) {
        return {};
    }
    // ESP32-P4 PPA has four fractional scaling bits. Round down to avoid cropping.
    const float fit = std::min(static_cast<float>(width) / sourceWidth, static_cast<float>(height) / sourceHeight);
    CameraPreviewTransform result;
    result.scale = std::floor(fit * 16.0f) / 16.0f;
    if (result.scale < 1.0f / 16.0f || result.scale >= 256.0f) {
        return {};
    }
    result.width = static_cast<int>(sourceWidth * result.scale);
    result.height = static_cast<int>(sourceHeight * result.scale);
    result.x = (width - result.width) / 2;
    result.y = (height - result.height) / 2;
    return result;
}

}  // namespace hal
