#pragma once
#include <hal/ota_policy.h>
#include <array>
#include <memory>
#include <new>
#include <cstdint>
#include <functional>
#include <string>

namespace hal {
constexpr size_t kOtaHeaderBytes = 288;
struct OtaSourceInfo { int httpStatus = 0; int64_t contentLength = -1; };
enum class OtaTransferOutcome { Complete, Cancelled, Failed };
struct OtaTransferResult {
    OtaTransferOutcome outcome = OtaTransferOutcome::Failed;
    std::string error;
    size_t bytesReceived = 0;
    size_t totalBytes = 0;
    bool progressKnown = false;
};
struct OtaTransferCallbacks {
    std::function<bool(const std::string&, OtaSourceInfo&, std::string&)> open;
    std::function<int(uint8_t*, size_t)> read;
    std::function<bool()> bodyComplete;
    std::function<bool(const uint8_t*, size_t, std::string&)> validateHeader;
    std::function<bool(size_t, std::string&)> beginImage;
    std::function<bool(const uint8_t*, size_t, std::string&)> writeImage;
    std::function<bool(std::string&)> validateImage;
    std::function<bool()> enterCommit;
    std::function<bool(std::string&)> selectBoot;
    std::function<void()> cleanup;
    std::function<bool()> cancelled;
    std::function<void(size_t, size_t, bool)> progress;
    std::function<void()> installing;
};
inline OtaTransferResult runOtaTransfer(const std::string& url, size_t capacity, const OtaTransferCallbacks& io)
{
    struct Cleanup { const OtaTransferCallbacks& io; ~Cleanup() { if (io.cleanup) io.cleanup(); } } cleanup {io};
    OtaTransferResult result;
    auto cancel = [&] {
        if (!io.cancelled()) return false;
        result.outcome = OtaTransferOutcome::Cancelled;
        result.error = "Update cancelled";
        return true;
    };
    if (!validOtaUrl(url)) { result.error = "Use an HTTPS URL without credentials"; return result; }
    if (capacity < kOtaHeaderBytes) { result.error = "No usable inactive OTA partition"; return result; }
    if (cancel()) return result;
    OtaSourceInfo source;
    if (!io.open(url, source, result.error)) { cancel(); return result; }
    if (cancel()) return result;
    if (source.httpStatus != 200) { result.error = "Firmware server must return HTTP 200; redirects are not allowed"; return result; }
    if (source.contentLength == 0 || (source.contentLength > 0 &&
        (static_cast<uint64_t>(source.contentLength) > capacity || source.contentLength < kOtaHeaderBytes))) {
        result.error = "Firmware size is invalid or exceeds OTA partition"; return result;
    }
    result.progressKnown = source.contentLength > 0;
    result.totalBytes = result.progressKnown ? static_cast<size_t>(source.contentLength) : 0;
    auto buffer = std::unique_ptr<std::array<uint8_t, 4096>>(new (std::nothrow) std::array<uint8_t, 4096> {});
    if (!buffer) { result.error = "Could not allocate OTA transfer buffer"; return result; }
    bool begun = false;
    size_t buffered = 0;
    for (;;) {
        if (cancel()) return result;
        const auto read = io.read(buffer->data() + buffered, buffer->size() - buffered);
        if (cancel()) return result;
        if (read < 0 || static_cast<size_t>(read) > buffer->size() - buffered) {
            result.error = "Firmware download failed"; return result;
        }
        if (!read) break;
        if (static_cast<size_t>(read) > capacity - result.bytesReceived ||
            (result.progressKnown && static_cast<size_t>(read) > result.totalBytes - result.bytesReceived)) {
            result.error = "Firmware exceeds declared size or OTA partition"; return result;
        }
        result.bytesReceived += static_cast<size_t>(read);
        buffered += static_cast<size_t>(read);
        if (io.progress) io.progress(result.bytesReceived, result.totalBytes, result.progressKnown);
        if (!begun) {
            if (buffered < kOtaHeaderBytes) continue;
            if (!io.validateHeader(buffer->data(), buffered, result.error)) { cancel(); return result; }
            if (cancel()) return result;
            if (!io.beginImage(result.totalBytes, result.error)) { cancel(); return result; }
            begun = true;
        }
        if (!io.writeImage(buffer->data(), buffered, result.error)) { cancel(); return result; }
        buffered = 0;
    }
    if (!begun || !io.bodyComplete() || (result.progressKnown && result.bytesReceived != result.totalBytes)) {
        result.error = "Firmware download is incomplete"; cancel(); return result;
    }
    if (cancel()) return result;
    if (io.installing) io.installing();
    if (cancel()) return result;
    if (!io.validateImage(result.error)) { cancel(); return result; }
    if (cancel()) return result;
    if (!io.enterCommit()) {
        result.outcome = OtaTransferOutcome::Cancelled; result.error = "Update cancelled"; return result;
    }
    if (!io.selectBoot(result.error)) return result;
    result.outcome = OtaTransferOutcome::Complete;
    result.error.clear();
    return result;
}
}
