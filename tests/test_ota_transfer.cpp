#include <hal/ota_transfer.h>
#include <hal/network_state.h>
#include <future>
#include <chrono>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <tuple>
#include <vector>

namespace {
using Outcome             = hal::OtaTransferOutcome;
constexpr const char* url = "https://firmware.example/m5stack_tab5.bin";

// Transport and flash boundaries are injected; control flow, bounds, progress,
// cancellation and cleanup are the same helper called by the Tab5 backend.
struct Transfer {
    std::vector<uint8_t> body = std::vector<uint8_t>(7000, 0x5a);
    std::vector<uint8_t> written;
    std::vector<std::string> events;
    std::vector<std::tuple<size_t, size_t, bool>> progress;
    int status       = 200;
    int64_t declared = 7000;
    size_t capacity = 8192, chunk = 113, offset = 0, beginSize = 0;
    bool completeBody = true, cancelled = false, commitAllowed = true;
    bool readBeyondBuffer = false, begun = false, validated = false, flashOpen = false;
    int cleanups = 0, aborts = 0, selections = 0;
    std::string failAt, cancelAt;
    hal::NetworkState* network = nullptr;

    Transfer()
    {
        // ESP-IDF image24 + first segment8 + app descriptor256. This fixture
        // covers header policy; the injected flash validator stands in for SDK
        // whole-image checksum/signature validation, not a bootability proof.
        body[0]           = 0xe9;
        body[1]           = 4;
        body[12]          = 0x12;  // ESP32-P4, current Tab5 SDK firmware chip id.
        body[13]          = 0;
        body[32]          = 0x32;
        body[33]          = 0x54;
        body[34]          = 0xcd;
        body[35]          = 0xab;
        const char name[] = "m5stack_tab5";
        std::memcpy(body.data() + 80, name, sizeof(name));
    }
    bool event(const std::string& name, std::string* error = nullptr)
    {
        events.push_back(name);
        if (cancelAt == name) {
            if (network)
                network->cancelOta();
            else
                cancelled = true;
        }
        if (failAt != name) return true;
        if (error) *error = "Injected " + name + " failure";
        return false;
    }
    hal::OtaTransferCallbacks callbacks()
    {
        hal::OtaTransferCallbacks io;
        io.open = [&](const std::string& requested, hal::OtaSourceInfo& info, std::string& error) {
            assert(requested == url);
            info = {status, declared};
            return event("open", &error);
        };
        io.read = [&](uint8_t* output, size_t bytes) {
            if (!event("read")) return -1;
            if (readBeyondBuffer) return static_cast<int>(bytes + 1);
            const auto size = std::min({chunk, bytes, body.size() - offset});
            std::memcpy(output, body.data() + offset, size);
            offset += size;
            return static_cast<int>(size);
        };
        io.bodyComplete = [&] {
            event("bodyComplete");
            return completeBody;
        };
        io.validateHeader = [&](const uint8_t* data, size_t bytes, std::string& error) {
            assert(bytes >= hal::kOtaHeaderBytes && data[0] == body[0]);
            return event("header", &error) && hal::validOtaImageHeader(data, bytes, 0x12, "m5stack_tab5", error);
        };
        io.beginImage = [&](size_t bytes, std::string& error) {
            beginSize = bytes;
            if (!event("begin", &error)) return false;
            begun = flashOpen = true;
            return true;
        };
        io.writeImage = [&](const uint8_t* data, size_t bytes, std::string& error) {
            assert(begun && !validated);
            if (!event("write", &error)) return false;
            written.insert(written.end(), data, data + bytes);
            return true;
        };
        io.validateImage = [&](std::string& error) {
            assert(written == body);
            // esp_ota_end consumes its image handle on success and failure.
            flashOpen = false;
            if (!event("validate", &error)) return false;
            validated = true;
            return true;
        };
        io.enterCommit = [&] {
            event("commit");
            return commitAllowed && (network ? network->enterOtaCommit() : !cancelled);
        };
        io.selectBoot = [&](std::string& error) {
            assert(validated);
            ++selections;
            return event("select", &error);
        };
        io.cleanup = [&] {
            event("cleanup");
            ++cleanups;
            if (flashOpen) {
                ++aborts;
                flashOpen = false;
            }
        };
        io.cancelled = [&] { return network ? network->otaCancelled() : cancelled; };
        io.progress  = [&](size_t bytes, size_t total, bool known) {
            event("progress");
            progress.emplace_back(bytes, total, known);
        };
        io.installing = [&] { event("installing"); };
        return io;
    }
    hal::OtaTransferResult run(const std::string& requested = url)
    {
        const auto result = hal::runOtaTransfer(requested, capacity, callbacks());
        assert(cleanups == 1 && events.back() == "cleanup");
        return result;
    }
    bool called(const std::string& name) const
    {
        return std::find(events.begin(), events.end(), name) != events.end();
    }
    size_t index(const std::string& name) const
    {
        const auto it = std::find(events.begin(), events.end(), name);
        assert(it != events.end());
        return static_cast<size_t>(it - events.begin());
    }
};

void test_success_and_real_progress()
{
    for (const bool known : {true, false}) {
        Transfer transfer;
        if (!known) transfer.declared = -1;
        const auto result = transfer.run();
        assert(result.outcome == Outcome::Complete && result.error.empty());
        assert(result.bytesReceived == transfer.body.size() && result.progressKnown == known);
        assert(result.totalBytes == (known ? transfer.body.size() : 0));
        assert(transfer.beginSize == (known ? transfer.body.size() : 0));
        assert(transfer.written == transfer.body && transfer.selections == 1 && transfer.aborts == 0);
        assert(transfer.index("header") < transfer.index("begin"));
        assert(transfer.index("begin") < transfer.index("write"));
        assert(transfer.index("write") < transfer.index("installing"));
        assert(transfer.index("installing") < transfer.index("validate"));
        assert(transfer.index("validate") < transfer.index("commit"));
        assert(transfer.index("commit") < transfer.index("select"));
        assert(transfer.index("select") < transfer.index("cleanup"));
        size_t previous = 0;
        for (auto [bytes, total, progressKnown] : transfer.progress) {
            assert(bytes > previous && bytes <= transfer.body.size());
            assert(total == result.totalBytes && progressKnown == known);
            previous = bytes;
        }
        assert(previous == transfer.body.size());
    }
}

void test_url_trust_policy_and_response_rejections()
{
    for (const auto* rejected :
         {"", "http://firmware.example/app.bin", "ftp://firmware.example/app.bin", "https://", "https:///app.bin",
          "https://user:password@example.com/app.bin", "https://example.com/app.bin#fragment",
          "https://example.com/a b.bin", "https://example.com/\\app.bin",
          "https://example.com/app.bin\r\nX-Header: injected"}) {
        assert(!hal::validOtaUrl(rejected));
        Transfer transfer;
        const auto result = transfer.run(rejected);
        assert(result.outcome == Outcome::Failed && !result.error.empty());
        assert(!transfer.called("open") && !transfer.called("begin") && transfer.selections == 0);
    }
    assert(hal::validOtaUrl(url));
    assert(hal::validOtaUrl("https://firmware.example:443/app.bin?signature=abc%2F123"));
    assert(!hal::validOtaUrl(std::string("https://example.com/") + std::string(2048, 'x')));
    for (const int status : {201, 204, 206, 301, 302, 303, 307, 308, 401, 403, 404, 500, 503}) {
        Transfer transfer;
        transfer.status   = status;
        const auto result = transfer.run();
        assert(result.outcome == Outcome::Failed && !result.error.empty());
        assert(!transfer.called("read") && !transfer.called("begin") && transfer.selections == 0);
    }
}

void test_size_and_incomplete_body_rejections()
{
    for (const int64_t declared : {int64_t{0}, int64_t{100}, int64_t{8193}}) {
        Transfer transfer;
        transfer.declared = declared;
        const auto result = transfer.run();
        assert(result.outcome == Outcome::Failed && !transfer.called("begin"));
        assert(!transfer.called("read") && transfer.selections == 0);
    }
    {
        Transfer transfer;
        transfer.capacity = hal::kOtaHeaderBytes - 1;
        assert(transfer.run().outcome == Outcome::Failed && !transfer.called("open"));
    }
    for (const bool known : {true, false}) {
        Transfer transfer;
        transfer.body.resize(200);  // A descriptor cannot be completed, even with unknown length.
        if (!known) transfer.declared = -1;
        assert(transfer.run().outcome == Outcome::Failed);
        assert(!transfer.called("begin") && !transfer.called("validate") && transfer.selections == 0);
    }
    {
        Transfer transfer;
        transfer.body.resize(5000);  // Declared 7000 but transport ends early.
        assert(transfer.run().outcome == Outcome::Failed);
        assert(transfer.begun && transfer.aborts == 1 && transfer.selections == 0);
    }
    for (const bool known : {true, false}) {
        Transfer transfer;
        transfer.body.resize(8500);
        if (!known) transfer.declared = -1;
        assert(transfer.run().outcome == Outcome::Failed && transfer.selections == 0);
        assert(transfer.written.size() <= transfer.capacity && transfer.aborts == 1);
    }
    {
        Transfer transfer;
        transfer.completeBody = false;
        assert(transfer.run().outcome == Outcome::Failed);
        assert(transfer.aborts == 1 && !transfer.called("validate") && transfer.selections == 0);
    }
    {
        Transfer transfer;
        transfer.readBeyondBuffer = true;
        assert(transfer.run().outcome == Outcome::Failed && !transfer.called("begin"));
    }
}

void test_transport_flash_and_validation_failures()
{
    for (const auto* stage : {"open", "read", "header", "begin", "write", "validate", "select"}) {
        Transfer transfer;
        transfer.failAt   = stage;
        const auto result = transfer.run();
        assert(result.outcome == Outcome::Failed && !result.error.empty());
        assert(transfer.selections == (std::string(stage) == "select" ? 1 : 0));
        assert(transfer.aborts == (std::string(stage) == "write" ? 1 : 0));
    }
}

void test_wrong_image_header_policy()
{
    // Exercise the same production parser used by the platform callback.
    for (const int mutation : {0, 1, 2, 3, 4, 5, 6}) {
        Transfer transfer;
        switch (mutation) {
            case 0:
                transfer.body[0] = '<';
                break;  // An HTML response with HTTP200.
            case 1:
                transfer.body[1] = 0;
                break;
            case 2:
                transfer.body[1] = 17;
                break;
            case 3:
                transfer.body[12] = 0;
                break;  // Wrong Espressif target.
            case 4:
                transfer.body[32] = 0;
                break;  // Missing app descriptor.
            case 5:
                transfer.body[80] = 'x';
                break;  // Different project.
            case 6:
                std::fill(transfer.body.begin() + 80, transfer.body.begin() + 112, 'x');
                break;
        }
        const auto result = transfer.run();
        assert(result.outcome == Outcome::Failed && !result.error.empty());
        assert(!transfer.called("begin") && !transfer.called("write") && transfer.selections == 0);
    }
    Transfer transfer;
    std::string error;
    assert(!hal::validOtaImageHeader(transfer.body.data(), hal::kOtaHeaderBytes - 1, 0x12, "m5stack_tab5", error));
}

void test_cancellation_before_commit()
{
    {
        Transfer transfer;
        transfer.cancelled = true;
        assert(transfer.run().outcome == Outcome::Cancelled && !transfer.called("open"));
    }
    for (const auto* stage :
         {"open", "read", "progress", "header", "begin", "write", "installing", "validate", "commit"}) {
        Transfer transfer;
        transfer.cancelAt = stage;
        const auto result = transfer.run();
        assert(result.outcome == Outcome::Cancelled && transfer.selections == 0);
        assert(!result.error.empty());
    }
    // A cancel arriving while a blocking precommit operation fails still
    // completes as Cancelled, rather than replacing the user's request with Failed.
    for (const auto* stage : {"open", "read", "header", "begin", "write", "validate"}) {
        Transfer transfer;
        transfer.cancelAt = transfer.failAt = stage;
        const auto result                   = transfer.run();
        assert(result.outcome == Outcome::Cancelled && transfer.selections == 0);
        assert(result.error == "Update cancelled");
    }
    {
        Transfer transfer;
        transfer.commitAllowed = false;
        assert(transfer.run().outcome == Outcome::Cancelled && transfer.selections == 0);
    }
}
void connect(hal::NetworkState& network)
{
    network.update([](auto& state) { state.wifi.state = hal::HalBase::WifiConnectionState::Connected; });
}

void test_production_network_reservation_and_commit_barrier()
{
    using namespace std::chrono;
    hal::NetworkState network;
    assert(!network.reserveOta());  // A disconnected radio cannot own an update.
    connect(network);
    assert(network.reserveOta());
    assert(!network.reserveOta() && !network.allowsCommands());
    Transfer transfer;
    transfer.network = &network;
    auto io          = transfer.callbacks();
    auto open        = io.open;
    auto cleanup     = io.cleanup;
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    io.open   = [&](const std::string& requested, hal::OtaSourceInfo& source, std::string& error) {
        entered.set_value();
        gate.wait();
        return open(requested, source, error);
    };
    io.cleanup = [&] {
        // Command ownership must survive cancellation and all resource cleanup.
        assert(!network.allowsCommands() && network.snapshot().otaActive);
        cleanup();
    };
    auto running = std::async(std::launch::async, [&] { return hal::runOtaTransfer(url, transfer.capacity, io); });
    assert(entered.get_future().wait_for(seconds(2)) == std::future_status::ready);
    auto readers = std::async(std::launch::async, [&] {
        for (int count = 0; count < 100; ++count) {
            assert(network.snapshot().otaActive);
            assert(!network.allowsCommands());
        }
        network.quiesce();
        assert(network.otaCancelled() && !network.enterOtaCommit());
    });
    assert(readers.wait_for(milliseconds(100)) == std::future_status::ready);
    readers.get();
    release.set_value();
    assert(running.wait_for(seconds(2)) == std::future_status::ready);
    assert(running.get().outcome == Outcome::Cancelled);
    assert(transfer.cleanups == 1 && transfer.selections == 0);
    network.update([](auto& state) { state.otaActive = false; });
    assert(!network.allowsCommands() && !network.reserveOta());
    network.resume();
    assert(network.allowsCommands() && network.reserveOta());

    // Cancellation wins immediately before commit: no partition selection.
    Transfer beforeCommit;
    beforeCommit.network  = &network;
    beforeCommit.cancelAt = "validate";
    assert(beforeCommit.run().outcome == Outcome::Cancelled && beforeCommit.selections == 0);
    network.update([](auto& state) { state.otaActive = false; });
    assert(network.reserveOta());
    // Once commit wins, a later cancellation request is explicitly refused.
    Transfer afterCommit;
    afterCommit.network  = &network;
    afterCommit.cancelAt = "select";
    assert(afterCommit.run().outcome == Outcome::Complete && afterCommit.selections == 1);
    assert(!network.otaCancelled() && !network.snapshot().ota.canCancel);
    network.update([](auto& state) {
        state.otaActive          = false;
        state.ota.rebootRequired = true;
    });
    assert(!network.reserveOta());
}

}  // namespace

int main()
{
    test_success_and_real_progress();
    test_url_trust_policy_and_response_rejections();
    test_size_and_incomplete_body_rejections();
    test_transport_flash_and_validation_failures();
    test_wrong_image_header_policy();
    test_cancellation_before_commit();
    test_production_network_reservation_and_commit_barrier();
}
