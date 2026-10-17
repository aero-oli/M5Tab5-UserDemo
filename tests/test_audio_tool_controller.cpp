#include <cassert>
#include <string>
#include <apps/app_media_audio/audio_tool_controller.h>

int main()
{
    using app_media_audio::AudioToolController;
    using app_media_audio::AudioToolKind;

    int dualStarts = 0;
    int dualStops = 0;
    app_media_audio::AudioToolCallbacks dualCallbacks;
    dualCallbacks.start = [&]() { ++dualStarts; };
    dualCallbacks.stop = [&]() { ++dualStops; };
    dualCallbacks.micState = [] { return hal::HalBase::MIC_TEST_RECORDING; };
    AudioToolController dual(AudioToolKind::DualMic, dualCallbacks);
    dual.start();
    auto sample = dual.sample();
    assert(dualStarts == 1);
    assert(sample.state == "Recording");
    assert(sample.detail == "Ready.");
    assert(dual.shouldStopOnClose());
    dual.stop();
    assert(dualStops == 1);

    int headphoneStarts = 0;
    app_media_audio::AudioToolCallbacks headphoneCallbacks;
    headphoneCallbacks.start = [&]() { ++headphoneStarts; };
    headphoneCallbacks.micState = [] { return hal::HalBase::MIC_TEST_PLAYING; };
    headphoneCallbacks.headphoneDetected = [] { return false; };
    AudioToolController headphone(AudioToolKind::Headphone, headphoneCallbacks);
    headphone.start();
    sample = headphone.sample();
    assert(headphoneStarts == 1);
    assert(sample.state == "Playing");
    assert(sample.detail == "No headset detected. The test can still be started.");

    int musicStarts = 0;
    int musicStops = 0;
    app_media_audio::AudioToolCallbacks musicCallbacks;
    musicCallbacks.start = [&]() { ++musicStarts; };
    musicCallbacks.stop = [&]() { ++musicStops; };
    musicCallbacks.musicState = [] { return hal::HalBase::MUSIC_PLAY_PLAYING; };
    AudioToolController music(AudioToolKind::Music, musicCallbacks);
    music.start();
    music.stop();
    sample = music.sample();
    assert(musicStarts == 1);
    assert(musicStops == 1);
    assert(sample.state == "Playing");
    assert(sample.detail == "Ready.");
    assert(music.shouldStopOnClose());

    hal::HalBase::AudioStatus status;
    app_media_audio::AudioToolCallbacks statusCallbacks;
    statusCallbacks.status = [&] { return status; };
    AudioToolController reporting(AudioToolKind::DualMic, statusCallbacks);
    status.busy = true;
    status.phase = hal::HalBase::AudioPhase::Cancelling;
    sample = reporting.sample();
    assert(sample.busy && sample.state == "Stopping");
    status.phase = hal::HalBase::AudioPhase::Failed;
    status.lastError = "Microphone unavailable";
    sample = reporting.sample();
    assert(sample.state == "Failed" && sample.detail == status.lastError);

    hal::HalBase::InterfaceData interfaces;
    app_media_audio::AudioToolCallbacks interfaceCallbacks;
    interfaceCallbacks.status = [] { return hal::HalBase::AudioStatus {}; };
    interfaceCallbacks.interfaceStatus = [&] { return interfaces; };
    AudioToolController headset(AudioToolKind::Headphone, interfaceCallbacks);
    assert(headset.sample().detail == "Headphone status unavailable.");
    interfaces.valid = true;
    interfaces.hasSample = true;
    interfaces.headphoneKnown = true;
    assert(headset.sample().detail == "No headset detected. The test can still be started.");
    interfaces.headphone = true;
    interfaces.simulated = true;
    assert(headset.sample().detail == "Simulated: Headset detected.");
    interfaces.valid = false;
    interfaces.error = "Expander timeout";
    assert(headset.sample().detail == "Headphone status unavailable: Expander timeout");

    assert(AudioToolController::micStateLabel(hal::HalBase::MIC_TEST_IDLE) == std::string("Idle"));
    assert(AudioToolController::musicStateLabel(hal::HalBase::MUSIC_PLAY_IDLE) == std::string("Idle"));

    return 0;
}
