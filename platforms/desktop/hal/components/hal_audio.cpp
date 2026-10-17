/* SPDX-License-Identifier: MIT */
#include "../hal_desktop.h"
#include <hal/audio_session.h>
#include <apps/runtime/async_job_runner.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <SDL2/SDL.h>

namespace {
using Operation = hal::HalBase::AudioOperation;
using Phase = hal::HalBase::AudioPhase;
using Token = hal::AudioSession::Token;
hal::AudioSession session;
app_runtime::AsyncJobRunner runner;
std::atomic<uint8_t> volume {20};
SDL_AudioDeviceID device = 0; // Only the reserved codec owner touches SDL audio.

bool pauseFor(Token token, uint32_t ms)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        if (session.cancelled(token)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return !session.cancelled(token);
}

bool prepareDevice(Token token)
{
    if (device) return true;
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        session.fail(token, "Audio initialization failed");
        return false;
    }
    SDL_AudioSpec desired {};
    desired.freq = 48000;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    device = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (!device) {
        session.fail(token, "Audio device unavailable");
        return false;
    }
    SDL_PauseAudioDevice(device, 0);
    return true;
}

bool waitQueue(Token token)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (SDL_GetQueuedAudioSize(device)) {
        if (session.cancelled(token)) return false;
        if (std::chrono::steady_clock::now() >= end) {
            session.fail(token, "Audio device stalled");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

void playPcm(Token token, std::vector<int16_t>& data)
{
    if (data.size() > 48000 * 2 * 10) {
        session.fail(token, "PCM playback exceeds 10 seconds");
        return;
    }
    if (session.cancelled(token) || !prepareDevice(token)) return;
    session.setPhase(token, Phase::Playing);
    hal::transferAudio(session, token, data.data(), data.size() * sizeof(int16_t),
                       [token](void* buffer, size_t length, size_t& done) {
        if (!waitQueue(token)) return false;
        const float scale = volume.load() / 100.0f;
        std::vector<int16_t> adjusted(static_cast<int16_t*>(buffer), static_cast<int16_t*>(buffer) + length / sizeof(int16_t));
        for (auto& sample : adjusted) sample = static_cast<int16_t>(sample * scale);
        if (SDL_QueueAudio(device, adjusted.data(), static_cast<Uint32>(length)) < 0) return false;
        done = length;
        return true;
    });
    waitQueue(token);
    SDL_ClearQueuedAudio(device);
}

template <typename Work>
void startWorker(Operation operation, Phase phase, Work work)
{
    const auto token = session.reserve(operation, phase);
    if (!token) return;
    if (!runner.start("Audio", [token, work = std::move(work)]() mutable {
        work(token);
        session.finish(token);
    })) session.startFailed(token);
}

void micTest(Token token)
{
    if (pauseFor(token, 3000)) {
        session.setPhase(token, Phase::Playing);
        pauseFor(token, 3000);
    }
}

hal::HalBase::MicTestState_t micState()
{
    const auto status = session.snapshot();
    if (!status.busy || (status.operation != Operation::DualMic && status.operation != Operation::HeadphoneMic))
        return hal::HalBase::MIC_TEST_IDLE;
    return status.phase == Phase::Recording ? hal::HalBase::MIC_TEST_RECORDING : hal::HalBase::MIC_TEST_PLAYING;
}
}  // namespace

void HalDesktop::setSpeakerVolume(uint8_t value) { volume.store(std::min<uint8_t>(100, value)); }
uint8_t HalDesktop::getSpeakerVolume() { return volume.load(); }
hal::HalBase::AudioStatus HalDesktop::audioGetStatus() { return session.snapshot(); }
void HalDesktop::audioCancel() { session.cancel(); }
bool HalDesktop::stopAudioAndWait(uint32_t timeoutMs) { return session.stopAndWait(timeoutMs); }
void HalDesktop::resumeAudio() { session.resume(); }

void HalDesktop::audioPlay(std::vector<int16_t>& data, bool async)
{
    const auto token = session.reserve(Operation::Tone, Phase::Starting);
    if (!token) return;
    if (data.size() > 48000 * 2 * 10) {
        session.fail(token, "PCM playback exceeds 10 seconds");
        session.finish(token);
        return;
    }
    auto work = [token, samples = data]() mutable {
        playPcm(token, samples);
        session.finish(token);
    };
    if (async) {
        if (!runner.start("Audio PCM", std::move(work))) session.startFailed(token);
    } else {
        work();
    }
}

void HalDesktop::audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float)
{
    const auto token = session.reserve(Operation::Record, Phase::Recording);
    if (!token) return;
    data.clear();
    if (durationMs > 10000) {
        session.fail(token, "Recording duration exceeds 10 seconds");
    } else {
        const size_t frames = static_cast<size_t>(48000) * durationMs / 1000;
        data.resize(frames * 4);
        for (size_t i = 0; i < frames && !session.cancelled(token); ++i) {
            const auto sample = static_cast<int16_t>(std::sin(2 * 3.141592653589793 * 500 * i / 48000) * 32767);
            for (int channel = 0; channel < 4; ++channel) data[i * 4 + channel] = sample;
        }
        if (session.cancelled(token)) data.clear();
    }
    session.finish(token);
}
void HalDesktop::startDualMicRecordTest() { startWorker(Operation::DualMic, Phase::Recording, micTest); }
void HalDesktop::startHeadphoneMicRecordTest() { startWorker(Operation::HeadphoneMic, Phase::Recording, micTest); }
hal::HalBase::MicTestState_t HalDesktop::getDualMicRecordTestState() { return micState(); }
hal::HalBase::MicTestState_t HalDesktop::getHeadphoneMicRecordTestState() { return micState(); }
void HalDesktop::startPlayMusicTest()
{
    startWorker(Operation::Music, Phase::Playing, [](Token token) { pauseFor(token, 5000); });
}
hal::HalBase::MusicPlayState_t HalDesktop::getMusicPlayTestState()
{
    const auto status = session.snapshot();
    return status.busy && (status.operation == Operation::Music || status.operation == Operation::Startup ||
                           status.operation == Operation::Shutdown) ? MUSIC_PLAY_PLAYING : MUSIC_PLAY_IDLE;
}
void HalDesktop::stopPlayMusicTest() { audioCancel(); }
void HalDesktop::playStartupSfx()
{
    startWorker(Operation::Startup, Phase::Playing, [](Token token) { pauseFor(token, 500); });
}
void HalDesktop::playShutdownSfx()
{
    startWorker(Operation::Shutdown, Phase::Playing, [](Token token) { pauseFor(token, 500); });
}
