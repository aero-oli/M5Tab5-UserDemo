#include <cassert>
#include "apps/app_pps/pps_input.h"

int main()
{
    float value = -1;
    assert(app_pps::parseSetpoint("12.345", 500, 30000, value) && value == 12.345f);
    assert(app_pps::parseSetpoint("0.500", 500, 30000, value) && value == 0.5f);
    assert(app_pps::parseSetpoint("30", 500, 30000, value) && value == 30.0f);
    assert(app_pps::parseSetpoint("0", 0, 5000, value) && value == 0.0f);
    assert(app_pps::parseSetpoint("2.123", 0, 5000, value) && value == 2.123f);
    for (const char* input : {"", "0.499", "30.001", "nan", "inf", "1e1", "-1", "+1", "1.0001", "1.", ".5", " 5", "5 ",
                              "1..2", "9999999999999999"}) {
        value = 7;
        assert(!app_pps::parseSetpoint(input, 500, 30000, value));
        assert(value == 7);  // Rejected input cannot become a staged setpoint.
    }
    assert(!app_pps::parseSetpoint("5.001", 0, 5000, value));
}
