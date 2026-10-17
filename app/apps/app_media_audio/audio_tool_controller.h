/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <functional>
#include <string>
#include <hal/hal.h>

namespace app_media_audio {

enum class AudioToolKind {
    DualMic,
    Headphone,
    Music,
};

struct AudioToolCallbacks {
    std::function<void()> start;
    std::function<void()> stop;
    std::function<hal::HalBase::MicTestState_t()> micState;
    std::function<hal::HalBase::MusicPlayState_t()> musicState;
    std::function<bool()> headphoneDetected;
};

struct AudioToolSample {
    std::string state;
    std::string detail;
};

class AudioToolController {
public:
    AudioToolController(AudioToolKind kind, AudioToolCallbacks callbacks);

    void start();
    void stop();
    AudioToolSample sample() const;
    bool shouldStopOnClose() const;

    static std::string micStateLabel(hal::HalBase::MicTestState_t state);
    static std::string musicStateLabel(hal::HalBase::MusicPlayState_t state);

private:
    AudioToolKind _kind;
    AudioToolCallbacks _callbacks;
};

}  // namespace app_media_audio
