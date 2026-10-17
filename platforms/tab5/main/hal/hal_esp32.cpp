/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal/hal_esp32.h"
#include <apps/runtime/performance_telemetry.h>
#include <hal/profiles/tab5_gpio.h>
#include <array>
#include <mutex>
#include <cmath>
#include <modules/rtc/rx8130_calendar.h>
#include <mooncake_log.h>
#include <cstring>
#include <functional>
#include <esp_timer.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <bsp/m5stack_tab5.h>
#include <lv_demos.h>

extern esp_lcd_touch_handle_t _lcd_touch_handle;

static const std::string _tag = "hal";

static i2c_master_bus_handle_t get_module_i2c_bus(hal::ModuleBus bus)
{
    if (bus == hal::ModuleBus::PortA) {
        return bsp_ext_i2c_get_handle();
    }
    return bsp_i2c_get_handle();
}

static bool with_i2c_device(hal::ModuleBus bus, uint8_t address, const std::function<esp_err_t(i2c_master_dev_handle_t)>& op)
{
    i2c_master_bus_handle_t bus_handle = get_module_i2c_bus(bus);
    if (bus_handle == nullptr) {
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = address,
        .scl_speed_hz    = 400000,
    };

    i2c_master_dev_handle_t dev_handle = nullptr;
    if (i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle) != ESP_OK) {
        return false;
    }

    esp_err_t ret = op(dev_handle);
    i2c_master_bus_rm_device(dev_handle);
    return ret == ESP_OK;
}

static void lvgl_read_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    if (_lcd_touch_handle == NULL) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    esp_lcd_touch_read_data(_lcd_touch_handle);
    bool touchpad_pressed =
        esp_lcd_touch_get_coordinates(_lcd_touch_handle, touch_x, touch_y, touch_strength, &touch_cnt, 1);
    // mclog::tagInfo(_tag, "touchpad pressed: {}", touchpad_pressed);

    if (!touchpad_pressed) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = touch_x[0];
        data->point.y = touch_y[0];
    }
}

void HalEsp32::init()
{
    mclog::tagInfo(_tag, "init");

    mclog::tagInfo(_tag, "camera init");
    bsp_cam_osc_init();

    mclog::tagInfo(_tag, "i2c init");
    bsp_i2c_init();

    mclog::tagInfo(_tag, "io expander init");
    i2c_master_bus_handle_t i2c_bus_handle = bsp_i2c_get_handle();
    bsp_io_expander_pi4ioe_init(i2c_bus_handle);

    setChargeQcEnable(true);
    delay(50);
    setChargeEnable(true);
    // setChargeEnable(false);

    mclog::tagInfo(_tag, "i2c scan");
    bsp_i2c_scan();

    mclog::tagInfo(_tag, "codec init");
    delay(200);
    bsp_codec_init();

    mclog::tagInfo(_tag, "imu init");
    imu_init();

    mclog::tagInfo(_tag, "ina226 init");
    if (!ina226.begin(i2c_bus_handle, 0x41) ||
        !ina226.configure(INA226_AVERAGES_16, INA226_BUS_CONV_TIME_1100US, INA226_SHUNT_CONV_TIME_1100US,
                          INA226_MODE_SHUNT_BUS_CONT) || !ina226.calibrate(0.005, 8.192)) {
        publishPowerMonitorError("Power monitor initialization failed");
        mclog::tagError(_tag, "Power monitor initialization failed");
    } else {
        updatePowerMonitorData();
    }

    mclog::tagInfo(_tag, "rx8130 init");
    clearRtcIrq(); // Preserves VLF; initialization is only an explicitly confirmed operation.
    updateRtcData();
    update_system_time();

    mclog::tagInfo(_tag, "display init");
    bsp_reset_tp();
    bsp_display_cfg_t cfg = {.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
                             .buffer_size   = BSP_LCD_H_RES * BSP_LCD_V_RES,
                             .double_buffer = true,
                             .flags         = {
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
                                 .buff_dma = false,
#else
                                 .buff_dma = true,
#endif
                                 .buff_spiram = true,
                                 .sw_rotate   = true,
                             }};
    lvDisp = bsp_display_start_with_config(&cfg);
    lv_display_set_rotation(lvDisp, LV_DISPLAY_ROTATION_90);
    bsp_display_backlight_on();

    // // Touchpad lvgl indev
    // mclog::tagInfo(_tag, "create lvgl touchpad indev");
    // lvTouchpad = lv_indev_create();
    // lv_indev_set_type(lvTouchpad, LV_INDEV_TYPE_POINTER);
    // lv_indev_set_read_cb(lvTouchpad, lvgl_read_cb);
    // lv_indev_set_display(lvTouchpad, lvDisp);

    mclog::tagInfo(_tag, "usb host init");
    bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);

    mclog::tagInfo(_tag, "hid init");
    hid_init();

    mclog::tagInfo(_tag, "rs485 init");
    rs485_init();

    mclog::tagInfo(_tag, "set gpio output capability");
    set_gpio_output_capability();

    bsp_display_unlock();
}

static const gpio_num_t _driver_gpios[] = {
    // EXT I2C
    GPIO_NUM_0,
    GPIO_NUM_1,
    // esp-hosted esp32c6
    GPIO_NUM_8,
    GPIO_NUM_9,
    GPIO_NUM_10,
    GPIO_NUM_11,
    GPIO_NUM_12,
    GPIO_NUM_13,
    GPIO_NUM_15,
    // Display
    GPIO_NUM_22,
    GPIO_NUM_23,
    // Audio
    GPIO_NUM_26,
    GPIO_NUM_27,
    GPIO_NUM_28,
    GPIO_NUM_29,
    GPIO_NUM_30,
    // SYS I2C
    GPIO_NUM_31,
    GPIO_NUM_32,
    // uSD card
    GPIO_NUM_39,
    GPIO_NUM_40,
    GPIO_NUM_41,
    GPIO_NUM_42,
    GPIO_NUM_43,
    GPIO_NUM_44,
};

void HalEsp32::set_gpio_output_capability()
{
    // gpio_set_drive_capability((gpio_num_t)48, GPIO_DRIVE_CAP_0);
    for (int i = 0; i < sizeof(_driver_gpios) / sizeof(_driver_gpios[0]); i++) {
        gpio_num_t gpio = _driver_gpios[i];
        esp_err_t ret   = gpio_set_drive_capability(gpio, GPIO_DRIVE_CAP_0);
        if (ret == ESP_OK) {
            printf("GPIO %d drive capability set to GPIO_DRIVE_CAP_0\n", gpio);
        } else {
            printf("Failed to set GPIO %d drive capability: %s\n", gpio, esp_err_to_name(ret));
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                                   System                                   */
/* -------------------------------------------------------------------------- */
#include <driver/temperature_sensor.h>
static temperature_sensor_handle_t _temp_sensor = nullptr;

void HalEsp32::delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t HalEsp32::millis()
{
    return esp_timer_get_time() / 1000;
}

int HalEsp32::getCpuTemp()
{
    int value = 0;
    readCpuTemp(value);
    return value;
}

bool HalEsp32::readCpuTemp(int& value)
{
    // Reopened PM controllers can overlap while an older sample is finishing.
    static std::mutex mutex;
    static bool enabled = false;
    std::lock_guard<std::mutex> lock(mutex);
    if (_temp_sensor == nullptr) {
        temperature_sensor_config_t config = {.range_min = 20, .range_max = 100};
        if (temperature_sensor_install(&config, &_temp_sensor) != ESP_OK) return false;
    }
    if (!enabled) {
        if (temperature_sensor_enable(_temp_sensor) != ESP_OK) {
            if (temperature_sensor_uninstall(_temp_sensor) == ESP_OK) _temp_sensor = nullptr;
            return false;
        }
        enabled = true;
    }
    float temperature = 0;
    if (temperature_sensor_get_celsius(_temp_sensor, &temperature) != ESP_OK || !std::isfinite(temperature)) return false;
    value = static_cast<int>(temperature);
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                   Display                                  */
/* -------------------------------------------------------------------------- */
void HalEsp32::setDisplayBrightness(uint8_t brightness)
{
    _current_lcd_brightness = std::clamp((int)brightness, 0, 100);
    mclog::tagInfo("hal", "set display brightness: {}%", _current_lcd_brightness);
    bsp_display_brightness_set(_current_lcd_brightness);
}

uint8_t HalEsp32::getDisplayBrightness()
{
    return _current_lcd_brightness;
}

namespace {
thread_local unsigned displayLockDepth = 0;
thread_local uint64_t displayLockStartUs = 0;
thread_local uint64_t displayLockWaitUs = 0;
}
void HalEsp32::lvglLock()
{
    const auto started = app_runtime::monotonicMicros();
    lvgl_port_lock(0);
    if (displayLockDepth++ == 0) {
        displayLockStartUs = app_runtime::monotonicMicros();
        displayLockWaitUs = displayLockStartUs - started;
    }
}

void HalEsp32::lvglUnlock()
{
    const bool outer = --displayLockDepth == 0;
    const auto held = app_runtime::monotonicMicros() - displayLockStartUs;
    lvgl_port_unlock();
    if (outer) app_runtime::globalPerformanceTelemetry().recordLock(displayLockWaitUs, held);
}

/* -------------------------------------------------------------------------- */
/*                                     RTC                                    */
/* -------------------------------------------------------------------------- */
void HalEsp32::clearRtcIrq()
{
    std::lock_guard<std::mutex> lock(_rtcIoMutex);
    modules::HalI2cDevice device(hal::ModuleBus::MBus, 0x32);
    rtc::Rx8130Calendar calendar(device);
    if (!calendar.clearWakeIrq()) mclog::tagError(_tag, "RTC IRQ clear failed: {}", calendar.error());
}

void HalEsp32::updateRtcData()
{
    std::lock_guard<std::mutex> lock(_rtcIoMutex);
    auto data = getRtcData();
    modules::HalI2cDevice device(hal::ModuleBus::MBus, 0x32);
    rtc::Rx8130Calendar calendar(device);
    tm actual{};
    data.valid = calendar.read(actual, data.needsInitialization);
    data.error = calendar.error();
    if (data.valid) { data.time = actual; data.hasSample = true; data.sampledAtMs = millis(); }
    publishRtcData(data);
}

bool HalEsp32::setRtcTimeChecked(const tm& time, bool initialize)
{
    std::lock_guard<std::mutex> lock(_rtcIoMutex);
    auto data = getRtcData();
    modules::HalI2cDevice device(hal::ModuleBus::MBus, 0x32);
    rtc::Rx8130Calendar calendar(device);
    tm actual{};
    data.valid = calendar.set(time, initialize, actual);
    data.error = calendar.error();
    if (data.valid) {
        data.time = actual; data.hasSample = true; data.needsInitialization = false; data.sampledAtMs = millis();
    }
    publishRtcData(data);
    if (data.valid) update_system_time();
    return data.valid;
}

void HalEsp32::update_system_time()
{
    const auto data = getRtcData();
    if (!data.valid) return;
    tm time = data.time;
    struct timeval now{};
    now.tv_sec = mktime(&time);
    if (now.tv_sec < 0) return;
    settimeofday(&now, nullptr);
}

/* -------------------------------------------------------------------------- */
/*                                   SD Card                                  */
/* -------------------------------------------------------------------------- */
#include <dirent.h>
#include <cerrno>
#include <hal/sd_scan.h>
#include <sys/types.h>

bool HalEsp32::isSdCardMounted()
{
    return _sdMounted.load();
}

hal::HalBase::SdScanResult HalEsp32::scanSdCardResult(const std::string& dirPath)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    return hal::scanSdCardSession(
        [&] {
            const auto error = bsp_sdcard_init("/sd", 25);
            if (error != ESP_OK)
                return SdScanResult{SdScanStatus::MountFailed, {}, std::string("Could not mount SD card: ") + esp_err_to_name(error)};
            _sdMounted.store(true);
            return SdScanResult{SdScanStatus::Success, {}, {}};
        },
        [&] {
            SdScanResult result{SdScanStatus::Success, {}, {}};
            const auto path = std::string("/sd/") + dirPath;
            DIR* dir = opendir(path.c_str());
            if (!dir) return SdScanResult{SdScanStatus::ReadFailed, {}, "Could not open SD directory"};
            for (;;) {
                errno = 0;
                auto* entry = readdir(dir);
                if (!entry) {
                    if (errno) result = {SdScanStatus::ReadFailed, {}, "Could not read SD directory"};
                    break;
                }
                const std::string name = entry->d_name;
                if (name == "." || name == "..") continue;
                if (!hal::appendSdEntry(result, {name, entry->d_type == DT_DIR})) break;
            }
            if (closedir(dir) != 0) result = {SdScanStatus::ReadFailed, {}, "Could not close SD directory"};
            return result;
        },
        [&] {
            const bool ok = bsp_sdcard_deinit("/sd") == ESP_OK;
            if (ok) _sdMounted.store(false);
            return ok;
        });
}

/* -------------------------------------------------------------------------- */
/*                                  Interface                                 */
/* -------------------------------------------------------------------------- */
void HalEsp32::updateInterfaceData()
{
    std::lock_guard<std::mutex> ioLock(_interfaceIoMutex);
    bool usbC = false;
    bool headphone = false;
    const auto result = bsp_read_interface_status(&usbC, &headphone);
    if (result != ESP_OK) {
        publishInterfaceError(std::string("Interface read failed: ") + esp_err_to_name(result));
        return;
    }
    InterfaceData data;
    data.usbC = usbC;
    data.headphone = headphone;
    data.usbCKnown = true;
    data.headphoneKnown = true;
    // HID-host events cannot establish general USB-A port presence.
    data.usbAKnown = false;
    data.valid = true;
    data.hasSample = true;
    data.sampledAtMs = millis();
    publishInterfaceData(data);
}

bool HalEsp32::usbCDetect()
{
    const auto data = getInterfaceData();
    return data.valid && data.usbCKnown && data.usbC;
}

bool HalEsp32::headPhoneDetect()
{
    const auto data = getInterfaceData();
    return data.valid && data.headphoneKnown && data.headphone;
}

std::vector<uint8_t> HalEsp32::i2cScan(bool isInternal)
{
    i2c_master_bus_handle_t i2c_bus_handle;
    std::vector<uint8_t> addrs;

    if (isInternal) {
        i2c_bus_handle = bsp_i2c_get_handle();
    } else {
        i2c_bus_handle = bsp_ext_i2c_get_handle();
    }

    esp_err_t ret;
    uint8_t address;

    for (int i = 16; i < 128; i += 16) {
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            ret     = i2c_master_probe(i2c_bus_handle, address, 50);
            if (ret == ESP_OK) {
                addrs.push_back(address);
            }
        }
    }

    return addrs;
}

bool HalEsp32::i2cProbe(hal::ModuleBus bus, uint8_t address)
{
    i2c_master_bus_handle_t bus_handle = get_module_i2c_bus(bus);
    if (bus_handle == nullptr) {
        return false;
    }
    return i2c_master_probe(bus_handle, address, 50) == ESP_OK;
}

bool HalEsp32::i2cReadRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, uint8_t* buffer, size_t length)
{
    return with_i2c_device(bus, address, [&](i2c_master_dev_handle_t dev_handle) {
        return i2c_master_transmit_receive(dev_handle, &reg, 1, buffer, length, 100);
    });
}

bool HalEsp32::i2cWriteRegister(hal::ModuleBus bus, uint8_t address, uint8_t reg, const uint8_t* data, size_t length)
{
    uint8_t write_buffer[17] = {};
    if (length + 1 > sizeof(write_buffer)) {
        return false;
    }

    write_buffer[0] = reg;
    memcpy(&write_buffer[1], data, length);

    return with_i2c_device(bus, address, [&](i2c_master_dev_handle_t dev_handle) {
        return i2c_master_transmit(dev_handle, write_buffer, length + 1, 100);
    });
}

void HalEsp32::initPortAI2c()
{
    mclog::tagInfo(_tag, "init port a i2c");
    bsp_ext_i2c_init();
}

void HalEsp32::deinitPortAI2c()
{
    mclog::tagInfo(_tag, "deinit port a i2c");
    bsp_ext_i2c_deinit();
}

namespace {
std::mutex s_gpioMutex;
std::array<hal::GpioOutputState, 55> s_gpioOutputs;
}

std::vector<hal::GpioPinInfo> HalEsp32::gpioGetPins()
{
    auto pins = hal::tab5ExpansionPins();
#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
    for (auto& pin : pins) {
#if CONFIG_ESP_CONSOLE_UART_DEFAULT
        const bool consolePin = pin.pin == 37 || pin.pin == 38;
#else
        const bool consolePin = pin.pin == CONFIG_ESP_CONSOLE_UART_TX_GPIO || pin.pin == CONFIG_ESP_CONSOLE_UART_RX_GPIO;
#endif
        if (consolePin) {
            pin.outputAllowed = false;
            pin.reservation = "Serial console";
        }
    }
#endif
    return pins;
}

hal::GpioClaimResult HalEsp32::gpioInitOutput(uint8_t pin)
{
    std::lock_guard<std::mutex> lock(s_gpioMutex);
    if (pin >= s_gpioOutputs.size() || !hal::gpioOutputAllowed(gpioGetPins(), pin) || s_gpioOutputs[pin].claimed) {
        return hal::GpioClaimResult::Rejected;
    }
    auto& state = s_gpioOutputs[pin];
    state.claimed = true;
    // Set the output latch before enabling drive to avoid briefly driving a previous high level.
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    state.levelKnown = gpio_set_level(static_cast<gpio_num_t>(pin), 0) == ESP_OK && gpio_config(&config) == ESP_OK;
    state.high = false;
    return state.levelKnown ? hal::GpioClaimResult::Claimed : hal::GpioClaimResult::Unknown;
}

bool HalEsp32::gpioSetLevel(uint8_t pin, bool level)
{
    std::lock_guard<std::mutex> lock(s_gpioMutex);
    if (pin >= s_gpioOutputs.size() || !hal::gpioOutputAllowed(gpioGetPins(), pin) ||
        !s_gpioOutputs[pin].claimed || !s_gpioOutputs[pin].levelKnown) {
        return false;
    }
    auto& state = s_gpioOutputs[pin];
    state.levelKnown = gpio_set_level(static_cast<gpio_num_t>(pin), level) == ESP_OK;
    if (state.levelKnown) {
        state.high = level;
    }
    return state.levelKnown;
}

bool HalEsp32::gpioReset(uint8_t pin)
{
    std::lock_guard<std::mutex> lock(s_gpioMutex);
    if (pin >= s_gpioOutputs.size() || !hal::gpioOutputAllowed(gpioGetPins(), pin) || !s_gpioOutputs[pin].claimed) {
        return false;
    }
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&config) != ESP_OK) {
        s_gpioOutputs[pin].levelKnown = false;
        return false;
    }
    s_gpioOutputs[pin] = {};
    return true;
}

hal::GpioOutputState HalEsp32::gpioGetOutputState(uint8_t pin)
{
    std::lock_guard<std::mutex> lock(s_gpioMutex);
    return pin < s_gpioOutputs.size() ? s_gpioOutputs[pin] : hal::GpioOutputState{};
}
