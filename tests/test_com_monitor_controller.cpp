#include <cassert>
#include <string>
#include <hal/hal.h>
#include <apps/app_hardware_com_monitor/com_monitor_controller.h>

int main()
{
    hal::HalBase::UartMonitorData_t data;
    for (char c : std::string("abc")) {
        data.rxQueue.push(static_cast<uint8_t>(c));
    }

    std::string sent;
    bool sentNewline = false;
    app_hardware_com_monitor::ComMonitorController controller(
        [&]() -> hal::HalBase::UartMonitorData_t& { return data; },
        [&](const std::string& msg, bool newLine) {
            sent = msg;
            sentNewline = newLine;
            return true;
        });

    auto sample = controller.drainRx();

    assert(sample.text == "abc");
    assert(sample.bytesRead == 3);
    assert(data.rxQueue.empty());

    for (int i = 0; i < 900; ++i) {
        data.rxQueue.push(static_cast<uint8_t>('x'));
    }
    sample = controller.drainRx();
    assert(sample.text.size() == 800);
    assert(sample.bytesRead == 800);
    assert(data.rxQueue.size() == 100);

    assert(controller.sendTestMessage());
    assert(sent == "hello from Tab5");
    assert(sentNewline);

    std::string burst(hal::UartMonitorState::kCapacity, 'a');
    data.appendRx(reinterpret_cast<const uint8_t*>(burst.data()), burst.size());
    while (!data.rxQueue.empty()) controller.drainRx();
    assert(controller.logText().size() == controller.kLogBytes);
    const std::string tail = "latest row\n";
    data.appendRx(reinterpret_cast<const uint8_t*>(tail.data()), tail.size());
    controller.drainRx();
    assert(controller.logText().size() == controller.kLogBytes);
    assert(controller.logText().substr(controller.logText().size() - tail.size()) == tail);
    return 0;
}
