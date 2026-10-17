#include <cassert>
#include <vector>

#include <apps/app_hardware_sd/sd_card_controller.h>

static void test_formats_missing_card()
{
    auto text = app_hardware_sd::SdCardController::formatResult(false, {});
    assert(text == "No SD card mounted.");
}

static void test_formats_empty_card()
{
    auto text = app_hardware_sd::SdCardController::formatResult(true, {});
    assert(text == "SD card is mounted but no files were found.");
}

static void test_formats_root_entries()
{
    std::vector<hal::HalBase::FileEntry_t> files = {
        {"firmware.bin", false},
        {"logs", true},
    };
    auto text = app_hardware_sd::SdCardController::formatResult(true, files);
    assert(text == "SD card root:\n      firmware.bin\n[DIR] logs");
}

int main()
{
    test_formats_missing_card();
    test_formats_empty_card();
    test_formats_root_entries();
    return 0;
}
