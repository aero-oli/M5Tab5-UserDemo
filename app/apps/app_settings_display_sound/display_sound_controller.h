/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace app_settings_display_sound {

enum class SettingId {
    Brightness,
    Volume,
};

struct DisplaySoundSnapshot {
    uint8_t brightness = 0;
    uint8_t volume     = 0;
};

class DisplaySoundController {
public:
    using ReadFn  = std::function<uint8_t()>;
    using WriteFn = std::function<void(uint8_t value)>;

    DisplaySoundController(ReadFn readBrightnessFn, ReadFn readVolumeFn, WriteFn writeBrightnessFn,
                           WriteFn writeVolumeFn);

    DisplaySoundSnapshot read() const;
    uint8_t adjustBrightness(int delta);
    uint8_t adjustVolume(int delta);

    static uint8_t clampPercent(int value);
    static std::string formatPercent(uint8_t value);

private:
    ReadFn _readBrightnessFn;
    ReadFn _readVolumeFn;
    WriteFn _writeBrightnessFn;
    WriteFn _writeVolumeFn;
};

}  // namespace app_settings_display_sound
