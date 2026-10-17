#pragma once
#include <apps/ui/ui_layout.h>
namespace app_hardware_rtc {
struct RtcLayout {
    app_ui::Rect content, description, timeCard, dateCard;
    int cardGap = 24;
    app_ui::Rect dateInput, timeInput, setButton, sourcePanel, sourceTitle, sourceDetail;
};
inline app_ui::Rect rtcContentRect()
{
    return {0, 0, app_ui::safeContentRect().w, app_ui::safeContentRect().h};
}
inline RtcLayout makeRtcLayout()
{
    RtcLayout layout;
    layout.content      = rtcContentRect();
    layout.description  = {0, 0, layout.content.w, 32};
    layout.timeCard     = {0, 52, 360, 124};
    layout.dateCard     = {384, 52, 360, 124};
    layout.dateInput    = {0, 208, 360, 64};
    layout.timeInput    = {384, 208, 360, 64};
    layout.setButton    = {776, 208, 336, 64};
    layout.sourcePanel  = {0, 304, 1112, 208};
    layout.sourceTitle  = {0, 0, 1080, 30};
    layout.sourceDetail = {0, 44, 1080, 132};
    return layout;
}
}  // namespace app_hardware_rtc
