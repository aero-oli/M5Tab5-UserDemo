#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <limits>
#include <string>
#include <vector>

#include "modules/i2c_device.h"
#include "modules/pps/pps_device.h"

class FakeI2cDevice : public modules::I2cDevice {
public:
    explicit FakeI2cDevice(bool present = true) : _present(present)
    {
    }

    bool probe() override
    {
        return _present;
    }

    uint8_t address() const override
    {
        return 0x35;
    }

    bool readRegister(uint8_t reg, uint8_t* buffer, size_t length) override
    {
        auto it = _registers.find(reg);
        if (!_present || it == _registers.end() || it->second.size() < length) {
            return false;
        }
        memcpy(buffer, it->second.data(), length);
        return true;
    }

    bool writeRegister(uint8_t reg, const uint8_t* data, size_t length) override
    {
        if (!_present) {
            return false;
        }
        _registers[reg] = std::vector<uint8_t>(data, data + length);
        return true;
    }

    void setBytes(uint8_t reg, std::vector<uint8_t> value)
    {
        _registers[reg] = value;
    }

    void setFloat(uint8_t reg, float value)
    {
        std::vector<uint8_t> bytes(4);
        memcpy(bytes.data(), &value, sizeof(value));
        setBytes(reg, bytes);
    }

    float writtenFloat(uint8_t reg)
    {
        float value = 0.0f;
        memcpy(&value, _registers[reg].data(), sizeof(value));
        return value;
    }

    uint8_t writtenByte(uint8_t reg)
    {
        return _registers[reg][0];
    }

private:
    bool _present;
    std::map<uint8_t, std::vector<uint8_t>> _registers;
};

static void seed_identity(FakeI2cDevice& fake)
{
    fake.setBytes(pps::REG_MODULE_ID_L, {0x41, 0x10});
    fake.setBytes(pps::REG_UID_W0, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C});
}

static void seed_telemetry(FakeI2cDevice& fake)
{
    fake.setBytes(pps::REG_MODULE_ENABLE, {0x01});
    fake.setBytes(pps::REG_RUNNING_MODE, {0x01});
    fake.setBytes(pps::REG_DATA_FLAG, {0x7B});
    fake.setFloat(pps::REG_VOUT_READBACK, 5.02f);
    fake.setFloat(pps::REG_IOUT_READBACK, 1.25f);
    fake.setFloat(pps::REG_TEMP_READBACK, 33.5f);
    fake.setFloat(pps::REG_VIN_READBACK, 24.1f);
    fake.setFloat(pps::REG_VOUT_SET, 5.0f);
    fake.setFloat(pps::REG_IOUT_SET, 1.5f);
}

static void test_reads_identity_and_telemetry()
{
    FakeI2cDevice fake;
    seed_identity(fake);
    seed_telemetry(fake);
    pps::PpsDevice pps(fake);

    pps::PpsIdentity identity;
    assert(pps.readIdentity(identity));
    assert(identity.moduleId == 0x1041);
    assert(identity.uid == "04030201-08070605-0C0B0A09");

    pps::PpsTelemetry telemetry;
    assert(pps.readTelemetry(telemetry));
    assert(telemetry.connected);
    assert(telemetry.outputEnabled);
    assert(telemetry.mode == pps::PpsMode::ConstantVoltage);
    assert(telemetry.dataFlag == 0x7B);
    assert(std::fabs(telemetry.voutReadback - 5.02f) < 0.001f);
    assert(std::fabs(telemetry.ioutReadback - 1.25f) < 0.001f);
    assert(std::fabs(telemetry.outputPower - 6.275f) < 0.001f);
}

static void test_clamps_setpoints_before_write()
{
    FakeI2cDevice fake;
    pps::PpsDevice pps(fake);

    assert(pps.writeSetpoints(45.0f, -2.0f));
    assert(std::fabs(fake.writtenFloat(pps::REG_VOUT_SET) - pps::MAX_OUTPUT_VOLTAGE) < 0.001f);
    assert(std::fabs(fake.writtenFloat(pps::REG_IOUT_SET) - pps::MIN_OUTPUT_CURRENT) < 0.001f);

    assert(pps.writeSetpoints(0.1f, 9.0f));
    assert(std::fabs(fake.writtenFloat(pps::REG_VOUT_SET) - pps::MIN_OUTPUT_VOLTAGE) < 0.001f);
    assert(std::fabs(fake.writtenFloat(pps::REG_IOUT_SET) - pps::MAX_OUTPUT_CURRENT) < 0.001f);
}

static void test_output_enable_is_separate()
{
    FakeI2cDevice fake;
    pps::PpsDevice pps(fake);

    assert(pps.setOutputEnabled(true));
    assert(fake.writtenByte(pps::REG_MODULE_ENABLE) == 0x01);
    assert(pps.setOutputEnabled(false));
    assert(fake.writtenByte(pps::REG_MODULE_ENABLE) == 0x00);
}

static void test_probe_failure_sets_error()
{
    FakeI2cDevice fake(false);
    pps::PpsDevice pps(fake);
    pps::PpsTelemetry telemetry;

    assert(!pps.readTelemetry(telemetry));
    assert(!telemetry.connected);
    assert(telemetry.lastError == "No PPS detected / check 9-36V input power");
}

static void test_non_finite_data_rejected_and_poll_can_be_interrupted()
{
    FakeI2cDevice fake;
    seed_telemetry(fake);
    pps::PpsDevice device(fake);
    assert(!device.writeSetpoints(std::numeric_limits<float>::quiet_NaN(), 1.0f));
    assert(!device.writeSetpoints(5.0f, std::numeric_limits<float>::infinity()));
    pps::PpsTelemetry telemetry;
    fake.setFloat(pps::REG_TEMP_READBACK, std::numeric_limits<float>::quiet_NaN());
    assert(!device.readTelemetry(telemetry));
    seed_telemetry(fake);
    int reads = 0;
    assert(!device.readTelemetry(telemetry, [&] { return ++reads > 2; }));
    assert(!telemetry.connected);
    fake.setBytes(pps::REG_VOUT_SET, {0x00, 0x00, 0xA0, 0x40});
    pps::PpsSetpoints setpoints;
    assert(device.readSetpoints(setpoints));
    assert(setpoints.voltage == 5.0f);
}

int main()
{
    test_reads_identity_and_telemetry();
    test_clamps_setpoints_before_write();
    test_output_enable_is_separate();
    test_probe_failure_sets_error();
    test_non_finite_data_rejected_and_poll_can_be_interrupted();
    return 0;
}
