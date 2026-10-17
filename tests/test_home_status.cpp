#include <apps/app_launcher/home_status.h>
#include <cassert>
#include <limits>

int main()
{
    using app_registry::StatusLevel;
    hal::HalBase::PMData_t power;
    assert(launcher_view::powerStatus(power, 0).text != "0.00V 0.00W");
    power.valid = power.hasSample = true;
    power.sampledAtMs             = 100;
    assert(launcher_view::powerStatus(power, 100).text == "0.00V 0.00W");  // Real zeros are valid.
    power.valid = false;
    assert(launcher_view::powerStatus(power, 200).level == StatusLevel::Warning);
    power.valid = true;
    assert(launcher_view::powerStatus(power, 2101).text == "Stale reading");
    power.sampledAtMs = std::numeric_limits<uint32_t>::max() - 10;
    power.simulated   = true;
    assert(launcher_view::powerStatus(power, 10).text.find("Sim ") == 0);
    assert(launcher_view::powerStatus(power, 10).level == StatusLevel::Neutral);

    hal::HalBase::RtcData rtc;
    assert(launcher_view::clockStatus(rtc, 0).text == "Clock unavailable");
    rtc.needsInitialization = true;
    assert(launcher_view::clockStatus(rtc, 0).text == "Clock invalid");
    rtc.needsInitialization = false;
    rtc.valid = rtc.hasSample = true;
    rtc.time.tm_hour          = 0;
    rtc.time.tm_min           = 0;
    assert(launcher_view::clockStatus(rtc, 0).text == "00:00");  // Verified midnight is different from unavailable.
    assert(launcher_view::clockStatus(rtc, 2001).text == "Clock stale");
    rtc.valid = false;
    assert(launcher_view::clockStatus(rtc, 1).text == "Clock unavailable");
}
