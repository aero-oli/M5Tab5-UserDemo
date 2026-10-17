#pragma once
#include <string>
#include <cctype>
#include <cstdint>
#include <cstring>
namespace hal {
inline bool validOtaUrl(const std::string& url)
{
    if (url.rfind("https://", 0) != 0 || url.size() > 512) return false;
    const auto end = url.find_first_of("/?#", 8);
    const auto authority = url.substr(8, end == std::string::npos ? end : end - 8);
    if (authority.empty() || authority.find('@') != std::string::npos || authority[0] == ':') return false;
    std::string port;
    if (authority[0] == '[') {
        const auto close = authority.find(']');
        if (close == std::string::npos || close <= 1) return false;
        for (size_t i = 1; i < close; ++i)
            if (!std::isxdigit(static_cast<unsigned char>(authority[i])) && authority[i] != ':' && authority[i] != '.') return false;
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') return false;
            port = authority.substr(close + 2);
            if (port.empty()) return false;
        }
    } else {
        const auto colon = authority.find(':');
        const auto host = authority.substr(0, colon);
        if (host.empty()) return false;
        for (unsigned char c : host) if (!std::isalnum(c) && c != '.' && c != '-') return false;
        if (colon != std::string::npos) {
            port = authority.substr(colon + 1);
            if (port.empty()) return false;
        }
    }
    if (!port.empty()) {
        unsigned value = 0;
        for (unsigned char c : port) {
            if (!std::isdigit(c) || value > 6553) return false;
            value = value * 10 + c - '0';
        }
        if (value == 0 || value > 65535) return false;
    }
    // No fragments, credentials, control characters, whitespace or backslashes.
    for (unsigned char c : url) if (std::iscntrl(c) || std::isspace(c) || c == '\\' || c == '#') return false;
    return true;
}
// ESP application image: 24-byte image header, 8-byte first segment header,
// then the 256-byte app descriptor. Platform static_asserts pin these offsets.
inline bool validOtaImageHeader(const uint8_t* data, size_t length, uint16_t chip,
                                const std::string& project, std::string& error)
{
    if (length < 288 || data[0] != 0xe9 || data[1] == 0 || data[1] > 16) {
        error = "Invalid ESP application image header"; return false;
    }
    if ((static_cast<uint16_t>(data[12]) | static_cast<uint16_t>(data[13]) << 8) != chip) {
        error = "Firmware targets a different chip"; return false;
    }
    if (data[32] != 0x32 || data[33] != 0x54 || data[34] != 0xcd || data[35] != 0xab) {
        error = "Firmware application descriptor missing"; return false;
    }
    const auto nameLength = strnlen(reinterpret_cast<const char*>(data + 80), 32);
    if (nameLength == 0 || nameLength == 32 || project != std::string(reinterpret_cast<const char*>(data + 80), nameLength)) {
        error = "Firmware belongs to a different project"; return false;
    }
    return true;
}

}
