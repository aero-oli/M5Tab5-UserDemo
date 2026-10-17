/* SPDX-License-Identifier: MIT */
#pragma once
namespace desktop_smoke {
// Pump the runtime's existing LVGL lifecycle timer; never dispatch Mooncake independently.
void configure(void (*pumpLifecycle)());
void afterUpdate();
bool isDone();
}  // namespace desktop_smoke
