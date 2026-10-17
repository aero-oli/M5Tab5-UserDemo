#include <cassert>
#include <apps/app_hardware_imu/imu_controller.h>

int main()
{
    bool updateCalled = false;
    hal::HalBase::IMUData_t data;
    data.accelX = 0.1234f;
    data.accelY = -0.5f;
    data.accelZ = 1.0f;
    data.gyroX = 12.25f;
    data.gyroY = -3.5f;
    data.gyroZ = 0.0f;

    app_hardware_imu::ImuController controller([&]() { updateCalled = true; }, [&]() { return data; });

    auto sample = controller.sample();

    assert(updateCalled);
    assert(sample.accelX == data.accelX);
    assert(sample.gyroY == data.gyroY);
    assert(app_hardware_imu::ImuController::formatAccel(sample.accelX) == "0.123 g");
    assert(app_hardware_imu::ImuController::formatAccel(sample.accelY) == "-0.500 g");
    assert(app_hardware_imu::ImuController::formatGyro(sample.gyroX) == "12.25 dps");

    return 0;
}
