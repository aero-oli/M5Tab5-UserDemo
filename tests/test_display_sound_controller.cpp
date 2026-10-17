#include <cassert>
#include <cstdint>
#include <vector>

#include <apps/app_settings_display_sound/display_sound_controller.h>

namespace {

struct SetCall {
    app_settings_display_sound::SettingId id;
    uint8_t value;
};

void test_reads_current_settings()
{
    app_settings_display_sound::DisplaySoundController controller(
        [] { return static_cast<uint8_t>(70); },
        [] { return static_cast<uint8_t>(30); },
        [](uint8_t) {},
        [](uint8_t) {});

    const auto snapshot = controller.read();

    assert(snapshot.brightness == 70);
    assert(snapshot.volume == 30);
}

void test_adjusts_brightness_in_steps_and_clamps()
{
    uint8_t brightness = 95;
    std::vector<SetCall> calls;
    app_settings_display_sound::DisplaySoundController controller(
        [&] { return brightness; },
        [] { return static_cast<uint8_t>(50); },
        [&](uint8_t value) {
            brightness = value;
            calls.push_back({app_settings_display_sound::SettingId::Brightness, value});
        },
        [](uint8_t) {});

    assert(controller.adjustBrightness(10) == 100);
    assert(controller.adjustBrightness(-200) == 0);
    assert(calls.size() == 2);
    assert(calls[0].value == 100);
    assert(calls[1].value == 0);
}

void test_adjusts_volume_in_steps_and_clamps()
{
    uint8_t volume = 5;
    std::vector<SetCall> calls;
    app_settings_display_sound::DisplaySoundController controller(
        [] { return static_cast<uint8_t>(50); },
        [&] { return volume; },
        [](uint8_t) {},
        [&](uint8_t value) {
            volume = value;
            calls.push_back({app_settings_display_sound::SettingId::Volume, value});
        });

    assert(controller.adjustVolume(-10) == 0);
    assert(controller.adjustVolume(150) == 100);
    assert(calls.size() == 2);
    assert(calls[0].value == 0);
    assert(calls[1].value == 100);
}

void test_formats_percent_values()
{
    assert(app_settings_display_sound::DisplaySoundController::formatPercent(0) == "0%");
    assert(app_settings_display_sound::DisplaySoundController::formatPercent(87) == "87%");
}

}  // namespace

int main()
{
    test_reads_current_settings();
    test_adjusts_brightness_in_steps_and_clamps();
    test_adjusts_volume_in_steps_and_clamps();
    test_formats_percent_values();
    return 0;
}
