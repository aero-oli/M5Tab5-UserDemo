# Extending the Tab5 console

The app registry, app-owned controllers and shared UI shell are the extension points. Add a feature
inside its own `app/apps/app_<feature>/` directory, then register it in `app/apps/app_catalog.cpp`
and add its `AppKey` in `app/apps/app_registry.h`. The launcher reads that metadata; adding an app
does not require changing launcher geometry. Reconfigure CMake after adding source files.

## Execution ownership

| Work | Owner | Contract |
| --- | --- | --- |
| Mooncake lifecycle and LVGL objects | The LVGL lifecycle timer | UI callbacks may request registry navigation on this same owner. No worker calls Mooncake or LVGL. |
| Sensor acquisition and device commands | Registered workers | Capture copied inputs and shared result/resource state, not a view or controller pointer. |
| UI data | Copied snapshots | Hold short state mutexes only while copying/publishing. Never hold them during I/O. |
| UART | One persistent driver worker | Own SDK reads/writes/cleanup; publish bounded queues. Stop and join before HAL destruction. |
| Audio | Exclusive audio session | Respect cancellation, codec/clock handoff and cleanup failures. |
| Camera | Camera lifecycle owner | A failed stop may leave DMA active. Retain uncertain resources until reboot. |
| PPS | Persistent PPS worker | Emergency Off takes priority; readback establishes state. Closing a view does not destroy outstanding I/O. |
| Wi-Fi/OTA | Network command lease and synchronized state | OTA owns the link until transport/flash cleanup. The boot-selection boundary closes cancellation atomically. |

`app/app.cpp` owns runtime setup, the lifecycle timer, instrumentation and shutdown. The desktop
journeys live in `platforms/desktop/test_support/ui_smoke.cpp`; their pump uses the same timer.
Firmware does not compile that harness. `app::Destroy()` closes views, requests safe cleanup,
drains persistent services and registered jobs, and only then destroys HAL. A failed bounded drain
refuses destruction. Add a drain hook if a new service cannot use `AsyncJobRunner`.

Do not perform I2C, storage scans, blocking audio, camera startup/stop or network transactions from
rendering, LVGL callbacks or a status provider. Enqueue the work and render a busy state immediately.
Use existing resource ownership (module I2C buses, GPIO claims, audio session, camera/network owner)
instead of a global lock that blocks unrelated devices. Never release a resource claimed by another app.

## Data and failure states

Use existing HAL result/snapshot patterns. A successful zero is a valid reading. A failed read retains
the previous sample and its original timestamp but marks the current attempt invalid. Distinguish
never sampled, fresh, stale, unavailable and simulated data. Getters return cached snapshots; the
worker performs acquisition. Time comparisons use unsigned elapsed milliseconds to handle wraparound.

Check each operation that establishes state. Do not turn a failed write into success by copying the
requested value into the snapshot. Preserve the primary failure and any cleanup/rollback failure.
PPS communication loss means output unknown, potentially live; an Off request alone is not proof of
electrical shutdown. RTC invalid-oscillator recovery requires explicit confirmation. Camera/codec
cleanup uncertainty must not permit unsafe reuse.

## UI and navigation

- Use `AppShell` and the shared UI/layout helpers. Put app controls inside its safe content area.
- Bind text fields to the shell's draft editor. Save stages a complete value; Cancel leaves the target
  unchanged. PPS uses an unconstrained target field plus strict parsing because LVGL emits intermediate
  character events when `max_length` or `accepted_chars` is set during `set_text`.
- Keep Back and safety actions reachable during busy, failure, confirmation and keyboard states. The
  shell header action is available for PPS Emergency Off and discards an editor draft before acting.
- Keep changing lists/logs in bounded scroll viewports, with primary controls outside. Bound stored
  entries/bytes too. Update text only when changed, and preserve user scroll position where possible.
- Separate staging, applying and enabling outputs. Invalid input cannot become a command. Confirmation
  callbacks must use the value the user reviewed or revalidate it before dispatch.
- On close, delete UI timers/objects and detach callbacks before disposing app-owned UI state. A worker
  may finish later only if it owns all remaining state and shutdown can drain it safely.

## Validation for a new feature

Start with focused controller/driver tests for success, absent device, timeout, failed write/readback,
repeated commands and cleanup. Use the actual production transaction helper at the hardware boundary;
do not merely reproduce its logic in a mock. Add a simulator journey for open, operate, Back during
work, reopen, error recovery and any long content. Check clipping and LVGL pointer targets, not just
rectangle containment. Add sanitizer coverage where lifetime/concurrency risk warrants it.

Run the desktop preset and Tab5 firmware build in [STABILIZATION.md](STABILIZATION.md). Preserve the
dependency pins; do not patch downloaded managed components. Board-specific fixes belong in the
tracked BSP or app adapters. Passing desktop/CI tests establishes software behavior only. Record
physical acceptance separately using [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).
