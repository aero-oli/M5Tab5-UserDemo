/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "display_sound_controller.h"

#include <algorithm>
#include <cstdio>

using namespace app_settings_display_sound;

DisplaySoundController::DisplaySoundController(ReadFn readBrightnessFn, ReadFn readVolumeFn, WriteFn writeBrightnessFn,
                                               WriteFn writeVolumeFn)
    : _readBrightnessFn(std::move(readBrightnessFn)), _readVolumeFn(std::move(readVolumeFn)),
      _writeBrightnessFn(std::move(writeBrightnessFn)), _writeVolumeFn(std::move(writeVolumeFn))
{
}

DisplaySoundSnapshot DisplaySoundController::read() const
{
    DisplaySoundSnapshot snapshot;
    snapshot.brightness = _readBrightnessFn();
    snapshot.volume = _readVolumeFn();
    return snapshot;
}

uint8_t DisplaySoundController::adjustBrightness(int delta)
{
    const auto next = clampPercent(static_cast<int>(_readBrightnessFn()) + delta);
    _writeBrightnessFn(next);
    return next;
}

uint8_t DisplaySoundController::adjustVolume(int delta)
{
    const auto next = clampPercent(static_cast<int>(_readVolumeFn()) + delta);
    _writeVolumeFn(next);
    return next;
}

uint8_t DisplaySoundController::clampPercent(int value)
{
    return static_cast<uint8_t>(std::clamp(value, 0, 100));
}

std::string DisplaySoundController::formatPercent(uint8_t value)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%u%%", static_cast<unsigned>(value));
    return buffer;
}
