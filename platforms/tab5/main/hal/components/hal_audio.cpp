/* SPDX-License-Identifier: MIT */
#include "hal/hal_esp32.h"
#include <hal/audio_session.h>
#include <apps/runtime/async_job_runner.h>
#include <algorithm>
#include <atomic>
#include <vector>
#include <bsp/m5stack_tab5.h>
#include <audio_player.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
using Operation = hal::HalBase::AudioOperation;
using Phase = hal::HalBase::AudioPhase;
using Token = hal::AudioSession::Token;

// Process-lifetime state: library callbacks never reference an app or HAL object.
hal::AudioSession session;
app_runtime::AsyncJobRunner runner;
std::atomic<uint8_t> volume {60};
hal::AudioVolume outputVolume; // Touched only by the exclusive output owner.
std::atomic<Token> musicToken {0};
std::atomic<bool> musicStarted {false};
std::atomic<bool> musicDone {false};
bool playerReady = false; // Only the reserved audio worker touches these flags.
bool callbackReady = false;
constexpr uint16_t maxRecordMs = 10000;
constexpr size_t maxPcmSamples = 48000 * 2 * 10;

bsp_codec_config_t* codec()
{
    return bsp_get_codec_handle();
}

bool check(Token token, esp_err_t result, const char* error)
{
    if (result == ESP_OK) return true;
    session.fail(token, error);
    return false;
}

bool applyOutputVolume(Token token)
{
    return outputVolume.apply(volume.load(), [token](uint8_t value) {
        return check(token, codec()->set_volume(value), "Audio volume setup failed");
    }, [token](bool muted) {
        return check(token, codec()->set_mute(muted), "Audio mute setup failed");
    });
}

bool prepareOutput(Token token)
{
    auto* handle = codec();
    if (!handle || !handle->set_volume || !handle->i2s_reconfig_clk_fn || !handle->i2s_write || !handle->set_mute) {
        session.fail(token, "Audio codec unavailable");
        return false;
    }
    outputVolume.invalidate();
    return check(token, handle->i2s_reconfig_clk_fn(48000, 16, I2S_SLOT_MODE_STEREO), "Audio clock setup failed") &&
           applyOutputVolume(token);
}

bool writePcm(Token token, void* data, size_t bytes)
{
    return hal::transferAudio(session, token, data, bytes, [token](void* buffer, size_t length, size_t& done) {
        if (!applyOutputVolume(token)) return false;
        return codec()->i2s_write(buffer, length, &done, 1000) == ESP_OK;
    });
}

bool recordPcm(Token token, std::vector<int16_t>& data, uint16_t durationMs, float gain)
{
    if (durationMs > maxRecordMs) {
        session.fail(token, "Recording duration exceeds 10 seconds");
        return false;
    }
    auto* handle = codec();
    if (!handle || !handle->i2s_read || !handle->set_in_gain || !handle->codec_reconfig_fn) {
        session.fail(token, "Audio input unavailable");
        return false;
    }
    // The BSP uses this callback for ES7210 sample-info.channel (four TDM
    // slots), despite its historical i2s_slot_mode_t parameter type.
    if (!check(token, handle->codec_reconfig_fn(48000, 16, static_cast<i2s_slot_mode_t>(4)),
               "Microphone clock setup failed")) return false;
    if (!check(token, handle->set_in_gain(gain), "Microphone gain setup failed")) return false;
    data.resize(static_cast<size_t>(48000) * 4 * durationMs / 1000);
    return hal::transferAudio(session, token, data.data(), data.size() * sizeof(int16_t),
                             [handle](void* buffer, size_t length, size_t& done) {
        return handle->i2s_read(buffer, length, &done, 1000) == ESP_OK;
    });
}

void finishPcm(Token token)
{
    auto* handle = codec();
    if (handle && handle->set_mute) outputVolume.mute([&](bool muted) {
        return check(token, handle->set_mute(muted), "Audio mute failed");
    });
    session.finish(token);
}

template <typename Work>
void startWorker(Operation operation, Phase phase, Work work)
{
    const auto token = session.reserve(operation, phase);
    if (!token) return;
    if (!runner.start("Audio", [token, work = std::move(work)]() mutable { work(token); }))
        session.startFailed(token);
}

void recordTest(Token token, bool dual)
{
    std::vector<int16_t> input;
    if (recordPcm(token, input, 3000, 240) && !session.cancelled(token)) {
        std::vector<int16_t> output(input.size() / 2);
        for (size_t frame = 0; frame < input.size() / 4; ++frame) {
            output[frame * 2] = input[frame * 4 + (dual ? 0 : 3)];
            output[frame * 2 + 1] = input[frame * 4 + (dual ? 2 : 3)];
        }
        session.setPhase(token, Phase::Playing);
        if (!session.cancelled(token) && prepareOutput(token))
            writePcm(token, output.data(), output.size() * sizeof(int16_t));
    }
    finishPcm(token);
}

esp_err_t musicMute(AUDIO_PLAYER_MUTE_SETTING setting)
{
    const auto token = musicToken.load();
    if (setting == AUDIO_PLAYER_UNMUTE && session.cancelled(token)) return ESP_FAIL;
    if (setting == AUDIO_PLAYER_UNMUTE) return applyOutputVolume(token) ? ESP_OK : ESP_FAIL;
    return outputVolume.mute([token](bool muted) {
        return check(token, codec()->set_mute(muted), "Audio mute failed");
    }) ? ESP_OK : ESP_FAIL;
}

esp_err_t musicClock(uint32_t rate, uint32_t bits, i2s_slot_mode_t channels)
{
    const auto token = musicToken.load();
    if (session.cancelled(token)) return ESP_FAIL;
    outputVolume.invalidate();
    const auto result = codec()->i2s_reconfig_clk_fn(rate, bits, channels);
    check(token, result, "Music clock setup failed");
    return result;
}

esp_err_t musicWrite(void* data, size_t length, size_t* written, uint32_t)
{
    *written = 0;
    const auto token = musicToken.load();
    if (!writePcm(token, data, length)) return ESP_FAIL;
    *written = length;
    return ESP_OK;
}

void musicEvent(audio_player_cb_ctx_t* event)
{
    if (event->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_PLAYING) musicStarted.store(true);
    if (event->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_IDLE && musicStarted.load()) musicDone.store(true);
    if (event->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN_FILE_TYPE)
        session.fail(musicToken.load(), "Unsupported music file");
}

bool ensurePlayer(Token token)
{
    if (!playerReady) {
        audio_player_config_t config = {
            .mute_fn = musicMute,
            .clk_set_fn = musicClock,
            .write_fn = musicWrite,
            .priority = 8,
            .coreID = 1,
        };
        if (!check(token, audio_player_new(config), "Music player initialization failed")) return false;
        // This player and callbacks live for the process. Its idle task waits on its
        // queue without touching the codec. Never call the vendor's delete(): it can
        // free live resources after only 500ms, less than the codec's 1s IO timeout.
        playerReady = true;
    }
    if (!callbackReady) {
        if (!check(token, audio_player_callback_register(musicEvent, nullptr), "Music callback registration failed"))
            return false;
        callbackReady = true;
    }
    return true;
}

void musicPlayback(Token token, const uint8_t* begin, const uint8_t* end)
{
    musicToken.store(token);
    musicStarted.store(false);
    musicDone.store(false);
    if (session.cancelled(token) || !prepareOutput(token) || !ensurePlayer(token)) {
        finishPcm(token);
        return;
    }
    FILE* file = fmemopen(const_cast<uint8_t*>(begin), end - begin, "rb");
    if (!file) {
        session.fail(token, "Could not open embedded music");
        finishPcm(token);
        return;
    }
    if (!check(token, audio_player_play(file), "Could not queue music")) {
        fclose(file); // The library owns the file only after successful enqueue.
        finishPcm(token);
        return;
    }
    session.setPhase(token, Phase::Playing);
    bool stopQueued = false;
    while (!musicDone.load()) {
        const auto status = session.snapshot();
        if ((session.cancelled(token) || !status.lastError.empty()) && !stopQueued)
            stopQueued = audio_player_stop() == ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // IDLE is emitted after mute and fclose. No library codec calls remain.
    finishPcm(token);
}

hal::HalBase::MicTestState_t micState()
{
    const auto status = session.snapshot();
    if (!status.busy || (status.operation != Operation::DualMic && status.operation != Operation::HeadphoneMic))
        return hal::HalBase::MIC_TEST_IDLE;
    return status.phase == Phase::Recording ? hal::HalBase::MIC_TEST_RECORDING : hal::HalBase::MIC_TEST_PLAYING;
}
}  // namespace

extern const uint8_t canon_in_d_mp3_start[] asm("_binary_canon_in_d_mp3_start");
extern const uint8_t canon_in_d_mp3_end[] asm("_binary_canon_in_d_mp3_end");
extern const uint8_t startup_sfx_mp3_start[] asm("_binary_startup_sfx_mp3_start");
extern const uint8_t startup_sfx_mp3_end[] asm("_binary_startup_sfx_mp3_end");
extern const uint8_t shutdown_sfx_mp3_start[] asm("_binary_shutdown_sfx_mp3_start");
extern const uint8_t shutdown_sfx_mp3_end[] asm("_binary_shutdown_sfx_mp3_end");

void HalEsp32::setSpeakerVolume(uint8_t value) { volume.store(std::min<uint8_t>(100, value)); }
uint8_t HalEsp32::getSpeakerVolume() { return volume.load(); }
hal::HalBase::AudioStatus HalEsp32::audioGetStatus() { return session.snapshot(); }
void HalEsp32::audioCancel() { session.cancel(); }
bool HalEsp32::stopAudioAndWait(uint32_t timeoutMs) { return session.stopAndWait(timeoutMs); }
void HalEsp32::resumeAudio() { session.resume(); }

void HalEsp32::audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float gain)
{
    const auto token = session.reserve(Operation::Record, Phase::Recording);
    if (!token) return;
    if (!recordPcm(token, data, durationMs, gain)) data.clear();
    session.finish(token);
}

void HalEsp32::audioPlay(std::vector<int16_t>& data, bool async)
{
    const auto token = session.reserve(Operation::Tone, Phase::Starting);
    if (!token) return;
    if (data.size() > maxPcmSamples) {
        session.fail(token, "PCM playback exceeds 10 seconds");
        session.finish(token);
        return;
    }
    auto work = [token, samples = data]() mutable {
        if (!session.cancelled(token) && prepareOutput(token)) {
            session.setPhase(token, Phase::Playing);
            writePcm(token, samples.data(), samples.size() * sizeof(int16_t));
        }
        finishPcm(token);
    };
    if (async) {
        if (!runner.start("Audio PCM", std::move(work))) session.startFailed(token);
    } else {
        work();
    }
}

void HalEsp32::startDualMicRecordTest()
{
    startWorker(Operation::DualMic, Phase::Recording, [](Token token) { recordTest(token, true); });
}
void HalEsp32::startHeadphoneMicRecordTest()
{
    startWorker(Operation::HeadphoneMic, Phase::Recording, [](Token token) { recordTest(token, false); });
}
hal::HalBase::MicTestState_t HalEsp32::getDualMicRecordTestState() { return micState(); }
hal::HalBase::MicTestState_t HalEsp32::getHeadphoneMicRecordTestState() { return micState(); }
void HalEsp32::startPlayMusicTest()
{
    startWorker(Operation::Music, Phase::Starting, [](Token token) { musicPlayback(token, canon_in_d_mp3_start, canon_in_d_mp3_end); });
}
hal::HalBase::MusicPlayState_t HalEsp32::getMusicPlayTestState()
{
    const auto status = session.snapshot();
    return status.busy && (status.operation == Operation::Music || status.operation == Operation::Startup ||
                           status.operation == Operation::Shutdown) ? MUSIC_PLAY_PLAYING : MUSIC_PLAY_IDLE;
}
void HalEsp32::stopPlayMusicTest() { audioCancel(); }
void HalEsp32::playStartupSfx()
{
    startWorker(Operation::Startup, Phase::Starting, [](Token token) { musicPlayback(token, startup_sfx_mp3_start, startup_sfx_mp3_end); });
}
void HalEsp32::playShutdownSfx()
{
    startWorker(Operation::Shutdown, Phase::Starting, [](Token token) { musicPlayback(token, shutdown_sfx_mp3_start, shutdown_sfx_mp3_end); });
}
