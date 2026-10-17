#include <cassert>
#include <hal/hal.h>

class InterfaceHal : public hal::HalBase {
public:
    int reads = 0;
    bool fail = false;
    void updateInterfaceData() override
    {
        ++reads;
        if (fail) {
            publishInterfaceError("Expander timeout");
            return;
        }
        InterfaceData data;
        data.valid          = true;
        data.hasSample      = true;
        data.usbCKnown      = true;
        data.headphoneKnown = true;
        data.usbC           = true;
        data.headphone      = true;
        data.sampledAtMs    = 123;
        publishInterfaceData(data);
    }
};

int main()
{
    InterfaceHal hal;
    auto data = hal.getInterfaceData();
    assert(!data.valid && !data.hasSample && !data.usbAKnown);
    assert(hal.reads == 0);
    hal.updateInterfaceData();
    data = hal.getInterfaceData();
    assert(data.valid && data.hasSample && data.usbC && data.headphone);
    assert(data.usbCKnown && data.headphoneKnown && !data.usbAKnown);
    for (int i = 0; i < 100; ++i) (void)hal.getInterfaceData();
    assert(hal.reads == 1);  // Rendering snapshots never triggers physical IO.
    hal.fail = true;
    hal.updateInterfaceData();
    data = hal.getInterfaceData();
    assert(!data.valid && data.hasSample && data.error == "Expander timeout");
    assert(data.sampledAtMs == 123 && data.usbC && data.headphone);  // Preserve last good sample.
    data.error = "mutated copy";
    assert(hal.getInterfaceData().error == "Expander timeout");
    hal.fail = false;
    hal.updateInterfaceData();
    data = hal.getInterfaceData();
    assert(data.valid && data.error.empty());
    return 0;
}
