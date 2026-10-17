/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_catalog.h"
#include "app_launcher/home_status.h"

#include <cstdio>
#include <memory>
#include <hal/hal.h>
#include <lvgl.h>
#include "app_diagnostics/app_diagnostics.h"
#include "app_hardware_com_monitor/app_com_monitor.h"
#include "app_hardware_gpio/app_gpio.h"
#include "app_hardware_i2c/app_i2c_scan.h"
#include "app_hardware_imu/app_imu.h"
#include "app_hardware_power_monitor/app_power_monitor.h"
#include "app_hardware_rtc/app_rtc.h"
#include "app_hardware_sd/app_sd_card.h"
#include "app_hardware_switches/app_switches.h"
#include "app_media_audio/app_audio_tool.h"
#include "app_media_camera/app_camera.h"
#include "app_pps/app_pps.h"
#include "app_pps/pps_controller.h"
#include "app_settings/app_settings.h"
#include "app_settings_network/app_network.h"
#include "app_settings_updates/app_updates.h"

using namespace app_registry;

void app_catalog::registerDefaultApps(AppRegistry& apps)
{
    if (!apps.all().empty()) {
        return;
    }

    apps.add({AppKey::Network,
              AppCategory::Settings,
              "Network",
              "Wi-Fi station, AP and antenna",
              "Wi-Fi",
              LV_SYMBOL_WIFI,
              10,
              115,
              false,
              true,
              [] { return app_settings_network::makeNetworkApp(); },
              [] {
                  auto status = GetHAL()->wifiGetStatus();
                  if (status.state == hal::HalBase::WifiConnectionState::Connected) {
                      return AppStatus{status.ssid.empty() ? "Connected" : status.ssid, StatusLevel::Ok};
                  }
                  if (status.state == hal::HalBase::WifiConnectionState::ApRunning) {
                      return AppStatus{"AP mode", StatusLevel::Warning};
                  }
                  if (status.state == hal::HalBase::WifiConnectionState::Failed) {
                      return AppStatus{"Failed", StatusLevel::Warning};
                  }
                  return AppStatus{"Offline", StatusLevel::Neutral};
              }});
    apps.add({AppKey::Updates,
              AppCategory::Settings,
              "Updates",
              "Manual URL firmware update",
              "OTA",
              LV_SYMBOL_DOWNLOAD,
              20,
              0,
              true,
              false,
              [] { return app_settings_updates::makeUpdatesApp(); },
              StatusProvider{}});
    apps.add({AppKey::DisplaySound,
              AppCategory::Settings,
              "Display & Sound",
              "Brightness and volume",
              "",
              LV_SYMBOL_VOLUME_MAX,
              30,
              80,
              false,
              true,
              [] { return app_settings::makeDisplaySoundApp(); },
              [] {
                  char text[24];
                  std::snprintf(text, sizeof(text), "%u%% / %u%%", static_cast<unsigned>(GetHAL()->getDisplayBrightness()),
                                static_cast<unsigned>(GetHAL()->getSpeakerVolume()));
                  return AppStatus{text, StatusLevel::Neutral};
              }});
    apps.add({AppKey::SettingsPower,
              AppCategory::Settings,
              "Power",
              "Sleep and shutdown",
              "Sleep",
              LV_SYMBOL_POWER,
              40,
              0,
              true,
              false,
              [] { return app_settings::makePowerSettingsApp(); },
              StatusProvider{}});
    apps.add({AppKey::About,
              AppCategory::Settings,
              "About",
              "Device and build information",
              GetHAL()->type(),
              LV_SYMBOL_SETTINGS,
              50,
              0,
              false,
              false,
              [] { return app_settings::makeAboutApp(); },
              StatusProvider{}});
    apps.add({AppKey::Diagnostics,
              AppCategory::Settings,
              "Diagnostics",
              "UI performance and background tasks",
              "Perf",
              LV_SYMBOL_LIST,
              60,
              0,
              false,
              false,
              [] { return app_diagnostics::makeDiagnosticsApp(); },
              StatusProvider{}});
    apps.add({AppKey::Rtc,
              AppCategory::Hardware,
              "RTC",
              "Clock, date and RTC setting",
              "",
              LV_SYMBOL_REFRESH,
              10,
              100,
              false,
              true,
              [] { return app_hardware_rtc::makeRtcApp(); },
              [] {
                  return launcher_view::clockStatus(GetHAL()->getRtcData(), GetHAL()->millis());
              }});
    apps.add({AppKey::Imu,
              AppCategory::Hardware,
              "IMU",
              "Motion sensor readout",
              "Accel",
              LV_SYMBOL_GPS,
              40,
              40,
              false,
              false,
              [] { return app_hardware_imu::makeImuApp(); },
              StatusProvider{}});
    apps.add({AppKey::PowerMonitor,
              AppCategory::Hardware,
              "Power Monitor",
              "Battery, bus and CPU telemetry",
              "",
              LV_SYMBOL_BATTERY_3,
              10,
              120,
              false,
              true,
              [] { return app_hardware_power_monitor::makePowerMonitorApp(); },
              [] {
                  return launcher_view::powerStatus(GetHAL()->getPowerMonitorData(), GetHAL()->millis());
              }});
    apps.add({AppKey::Switches,
              AppCategory::Hardware,
              "Switches",
              "Power rails and interface toggles",
              "",
              LV_SYMBOL_USB,
              20,
              90,
              false,
              true,
              [] { return app_hardware_switches::makeSwitchesApp(); },
              [] {
                  const auto data = GetHAL()->getInterfaceData();
                  if (!data.valid || !data.hasSample || !data.usbCKnown || GetHAL()->millis() - data.sampledAtMs > 2000)
                      return AppStatus{"Ports unknown", StatusLevel::Warning};
                  return AppStatus{std::string(data.simulated ? "Sim " : "") +
                      (data.usbC ? "USB-C connected" : "USB-C absent"), StatusLevel::Neutral};
              }});
    apps.add({AppKey::I2cScan,
              AppCategory::Hardware,
              "I2C Scan",
              "Scan internal and external buses",
              "Scan",
              LV_SYMBOL_LIST,
              40,
              30,
              false,
              false,
              [] { return app_hardware_i2c::makeI2cScanApp(); },
              StatusProvider{}});
    apps.add({AppKey::Gpio,
              AppCategory::Hardware,
              "GPIO",
              "M-Bus and Port A GPIO test",
              "Test",
              LV_SYMBOL_KEYBOARD,
              50,
              20,
              true,
              false,
              [] { return app_hardware_gpio::makeGpioApp(); },
              StatusProvider{}});
    apps.add({AppKey::Camera,
              AppCategory::Media,
              "Camera",
              "Camera preview test",
              "Preview",
              LV_SYMBOL_IMAGE,
              10,
              20,
              false,
              false,
              [] { return app_media_camera::makeCameraApp(); },
              StatusProvider{}});
    apps.add({AppKey::DualMic,
              AppCategory::Media,
              "Dual Mic",
              "Record and play dual mic test",
              "Record",
              LV_SYMBOL_AUDIO,
              20,
              20,
              false,
              false,
              [] { return app_media_audio::makeDualMicApp(); },
              StatusProvider{}});
    apps.add({AppKey::Headphone,
              AppCategory::Media,
              "Headphone",
              "Headphone mic and output test",
              "",
              LV_SYMBOL_AUDIO,
              30,
              60,
              false,
              true,
              [] { return app_media_audio::makeHeadphoneApp(); },
              [] {
                  const auto data = GetHAL()->getInterfaceData();
                  if (!data.valid || !data.hasSample || !data.headphoneKnown || GetHAL()->millis() - data.sampledAtMs > 2000)
                      return AppStatus{"Detection unknown", StatusLevel::Warning};
                  return AppStatus{std::string(data.simulated ? "Sim " : "") +
                      (data.headphone ? "Detected" : "Not detected"), StatusLevel::Neutral};
              }});
    apps.add({AppKey::Music,
              AppCategory::Media,
              "Music",
              "Speaker playback test",
              "Play",
              LV_SYMBOL_PLAY,
              40,
              10,
              false,
              false,
              [] { return app_media_audio::makeMusicApp(); },
              StatusProvider{}});
    apps.add({AppKey::SdCard,
              AppCategory::Hardware,
              "SD Card",
              "Scan SD card files",
              "Open to scan",
              LV_SYMBOL_SD_CARD,
              10,
              50,
              false,
              true,
              [] { return app_hardware_sd::makeSdCardApp(); },
              StatusProvider{}});
    apps.add({AppKey::ComMonitor,
              AppCategory::Hardware,
              "COM Monitor",
              "RS485 serial monitor",
              "UART",
              LV_SYMBOL_KEYBOARD,
              20,
              20,
              false,
              false,
              [] { return app_hardware_com_monitor::makeComMonitorApp(); },
              StatusProvider{}});
    apps.add({AppKey::Pps,
              AppCategory::Modules,
              "PPS",
              "Module13.2 programmable supply",
              "0x35",
              LV_SYMBOL_CHARGE,
              10,
              110,
              false,
              true,
              [] { return std::make_unique<AppPps>(); },
              [] {
                  auto state = app_pps::PpsController::backgroundState();
                  if (!state) return AppStatus{"Not checked", StatusLevel::Neutral};
                  const auto level = state->off == app_pps::OffState::Unconfirmed ? StatusLevel::Error :
                      state->off == app_pps::OffState::Pending || state->output == app_pps::OutputState::Unknown
                          ? StatusLevel::Warning : StatusLevel::Neutral;
                  return AppStatus{app_pps::PpsController::outputSummary(*state), level};
              }});
}
