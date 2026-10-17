/* SPDX-License-Identifier: MIT */
#include "hal/hal_esp32.h"
#include <hal/uart_monitor.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <new>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr uart_port_t port = UART_NUM_1;
constexpr int txPin = 20, rxPin = 21, dePin = 34;
using State = hal::UartMonitorState;

void uartWorker(void* arg)
{
    {
        auto state = std::move(*static_cast<std::shared_ptr<State>*>(arg));
        delete static_cast<std::shared_ptr<State>*>(arg);
        hal::UartMonitorWorker worker(state);
        while (worker.poll(
            [](uint8_t* data, size_t size) { return uart_read_bytes(port, data, size, pdMS_TO_TICKS(100)); },
            [](const uint8_t* data, size_t size) {
                return uart_tx_chars(port, reinterpret_cast<const char*>(data), size);
            })) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // Cancellation stops admission first. No HAL object or global GetHAL()
        // is used by this task, and completion follows all driver cleanup.
        uart_wait_tx_done(port, pdMS_TO_TICKS(200));
        state->finishWorker(uart_driver_delete(port) == ESP_OK);
    }
    vTaskDelete(nullptr);
}
}  // namespace

void HalEsp32::rs485_init()
{
    const auto state = _uartMonitorState;
    bool installed   = false;
    const auto fail  = [&](const char* detail) {
        state->fail(detail);
        const bool clean = !installed || uart_driver_delete(port) == ESP_OK;
        state->finishWorker(clean);
        ESP_LOGE("hal_rs485", "%s", detail);
    };
    const auto result = uart_driver_install(port, 254, 0, 0, nullptr, 0);
    if (result != ESP_OK) {
        fail("UART driver installation failed");
        return;
    }
    installed = true;
    uart_config_t config{};
    config.baud_rate           = 115200;
    config.data_bits           = UART_DATA_8_BITS;
    config.parity              = UART_PARITY_DISABLE;
    config.stop_bits           = UART_STOP_BITS_1;
    config.flow_ctrl           = UART_HW_FLOWCTRL_DISABLE;
    config.rx_flow_ctrl_thresh = 122;
    config.source_clk          = UART_SCLK_DEFAULT;
    if (uart_param_config(port, &config) != ESP_OK ||
        uart_set_pin(port, txPin, rxPin, dePin, UART_PIN_NO_CHANGE) != ESP_OK ||
        uart_set_mode(port, UART_MODE_RS485_HALF_DUPLEX) != ESP_OK || uart_set_rx_timeout(port, 3) != ESP_OK) {
        fail("UART configuration failed");
        return;
    }
    auto* context = new (std::nothrow) std::shared_ptr<State>(state);
    if (!context) {
        fail("UART worker allocation failed");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->workerRunning = state->driverActive = true;
    }
    if (xTaskCreate(uartWorker, "rs485", 4096, context, 5, nullptr) != pdPASS) {
        delete context;
        fail("UART worker creation failed");
    }
}

bool HalEsp32::stopUartAndWait(uint32_t timeoutMs)
{
    const auto state = _uartMonitorState;
    state->requestStop();
    if (!state->waitForWorker(timeoutMs)) return false;
    bool active;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        active = state->driverActive;
    }
    if (!active) return true;
    // Retry a failed initialization/worker cleanup without discarding ownership.
    const bool clean = uart_driver_delete(port) == ESP_OK;
    state->finishWorker(clean);
    return clean;
}
