#include <cassert>
#include <cmath>
#include <ina226.hpp>

static int reads = 0, failAt = 0;
static bool failAdd = false, failWrite = false;
static uint16_t registers[8] = {};
extern "C" esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t*,
                                               i2c_master_dev_handle_t* handle)
{
    if (failAdd) {
        *handle = nullptr;
        return ESP_FAIL;
    }
    *handle = reinterpret_cast<void*>(1);
    return ESP_OK;
}
extern "C" esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t*, size_t, int)
{
    return failWrite ? ESP_FAIL : ESP_OK;
}
extern "C" esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t, const uint8_t* reg, size_t, uint8_t* output,
                                                 size_t, int)
{
    if (++reads == failAt) return ESP_FAIL;
    output[0] = registers[*reg] >> 8;
    output[1] = registers[*reg] & 0xff;
    return ESP_OK;
}

int main()
{
    INA226 missing;
    failAdd = true;
    assert(!missing.begin(nullptr));  // Device-admission failure must not abort the process.
    failAdd = false;
    INA226 sensor;
    float voltage = 99, power = 99, shunt = 99, current = 99;
    assert(!sensor.readMeasurements(voltage, power, shunt, current));
    assert(sensor.begin(nullptr));
    failWrite = true;
    assert(!sensor.configure());
    assert(!sensor.calibrate());
    assert(!sensor.readMeasurements(voltage, power, shunt, current));
    failWrite = false;
    assert(sensor.configure());
    assert(sensor.calibrate());
    failWrite = true;
    assert(!sensor.calibrate());
    assert(!sensor.readMeasurements(voltage, power, shunt, current));
    failWrite = false;
    assert(sensor.calibrate());
    // A genuinely all-zero transaction is a successful measurement.
    assert(sensor.readMeasurements(voltage, power, shunt, current));
    assert(voltage == 0 && power == 0 && shunt == 0 && current == 0);
    for (int failedRegister = 1; failedRegister <= 4; ++failedRegister) {
        reads   = 0;
        failAt  = failedRegister;
        voltage = power = shunt = current = 99;
        assert(!sensor.readMeasurements(voltage, power, shunt, current));
        assert(voltage == 99 && power == 99 && shunt == 99 && current == 99);
    }
    failAt                             = 0;
    registers[INA226_REG_BUSVOLTAGE]   = 8000;
    registers[INA226_REG_POWER]        = 40000;  // Unsigned power register must stay positive.
    registers[INA226_REG_SHUNTVOLTAGE] = static_cast<uint16_t>(-100);
    registers[INA226_REG_CURRENT]      = static_cast<uint16_t>(-100);
    assert(sensor.readMeasurements(voltage, power, shunt, current));
    assert(std::fabs(voltage - 10.0f) < 0.001f && power > 0 && shunt < 0 && current < 0);
}
