#include <cassert>
#include <vector>

#include <apps/app_hardware_i2c/i2c_scan_controller.h>

static void test_formats_empty_scan()
{
    auto text = app_hardware_i2c::I2cScanController::formatResult(false, {});
    assert(text == "Port A devices:\nNo devices found.");
}

static void test_formats_found_devices()
{
    auto text = app_hardware_i2c::I2cScanController::formatResult(true, {0x35, 0x68});
    assert(text == "Internal bus devices:\n0x35\n0x68");
}

int main()
{
    test_formats_empty_scan();
    test_formats_found_devices();
    return 0;
}
