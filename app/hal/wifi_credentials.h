#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cctype>
namespace hal {
struct WifiCredentialBytes {
    std::array<uint8_t, 32> ssid{};
    std::array<uint8_t, 64> password{};
};
inline bool encodeWifiCredentials(const std::string& ssid, const std::string& password, WifiCredentialBytes& result)
{
    if (ssid.empty() || ssid.size() > 32 || ssid.find('\0') != std::string::npos || password.size() > 64 ||
        password.find('\0') != std::string::npos || (!password.empty() && password.size() < 8))
        return false;
    if (password.size() == 64 &&
        !std::all_of(password.begin(), password.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
        return false;
    result = {};
    std::copy(ssid.begin(), ssid.end(), result.ssid.begin());
    std::copy(password.begin(), password.end(), result.password.begin());
    return true;
}
}  // namespace hal
