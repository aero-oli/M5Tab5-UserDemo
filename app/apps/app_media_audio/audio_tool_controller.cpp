/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "audio_tool_controller.h"

#include <utility>

using namespace app_media_audio;

AudioToolController::AudioToolController(AudioToolKind kind, AudioToolCallbacks callbacks)
    : _kind(kind), _callbacks(std::move(callbacks))
{
}

void AudioToolController::start()
{
    if (_callbacks.start) {
        _callbacks.start();
    }
}

void AudioToolController::stop()
{
    if (_callbacks.stop) {
        _callbacks.stop();
    }
}

AudioToolSample AudioToolController::sample() const
{
    AudioToolSample result;
    result.detail = "Ready.";
    if (_kind == AudioToolKind::Music) {
        result.state = musicStateLabel(_callbacks.musicState ? _callbacks.musicState() : hal::HalBase::MUSIC_PLAY_IDLE);
        return result;
    }

    result.state = micStateLabel(_callbacks.micState ? _callbacks.micState() : hal::HalBase::MIC_TEST_IDLE);
    if (_kind == AudioToolKind::Headphone && _callbacks.headphoneDetected && !_callbacks.headphoneDetected()) {
        result.detail = "No headset detected. The test can still be started.";
    }
    return result;
}

bool AudioToolController::shouldStopOnClose() const
{
    return _kind == AudioToolKind::Music;
}

std::string AudioToolController::micStateLabel(hal::HalBase::MicTestState_t state)
{
    switch (state) {
        case hal::HalBase::MIC_TEST_RECORDING:
            return "Recording";
        case hal::HalBase::MIC_TEST_PLAYING:
            return "Playing";
        case hal::HalBase::MIC_TEST_IDLE:
        default:
            return "Idle";
    }
}

std::string AudioToolController::musicStateLabel(hal::HalBase::MusicPlayState_t state)
{
    return state == hal::HalBase::MUSIC_PLAY_PLAYING ? "Playing" : "Idle";
}
