# Tab5 stabilization

## Recovery point

The pre-existing modernization is preserved in commit `7faf65f21028e1c841ce9741b342c99713961c14`
and tag `baseline/tab5-before-stabilization-20261017`. Work continues on
`codex/stabilize-tab5-20261017`. This checkpoint includes all 211 previously changed files.
Do not reset the working tree to this tag to inspect it; use `git show` or a separate checkout.

Baseline verified on 2026-10-17: desktop configure/build, 55/55 CTest tests, Tab5 firmware build,
and desktop rendered screen inspection. No device was flashed. Desktop audio output was unavailable.

Dependency revisions used for that baseline:

| Dependency | Revision |
| --- | --- |
| ESP-IDF | v5.4.2 |
| LVGL | 7f07a129e8d77f4984fff8e623fd5be18ff42e74 |
| Mooncake | 0dfc177b72fcc55095fd222b0bdc69595b001d9f |
| Mooncake log | 41d00d9d7209668215cea09dbfb0cbac36b5a1c0 |
| Smooth UI toolkit | 3a749e5b7090c6aba81729cf556b7b88cb42ce54 |

## Milestones and gates

Each implementation milestone gets focused regressions, desktop integration and a firmware build
before the next milestone. Device-only acceptance remains explicitly pending until exercised.
Do not describe a desktop simulator pass as electrical, DMA, wake, radio or OTA acceptance.

| Milestone | Scope | Status |
| --- | --- | --- |
| 1 | Preserve baseline, record builds and feature parity | Source checkpoint verified |
| 2 | Single UI lifecycle owner, synchronized network snapshots, nonblocking sensor sampling, worker shutdown and device instrumentation | Software gate passed: 56/56 desktop tests; Tab5 build |
| 3 | Camera failed-stop safety, BMI270 wake rollback, audio ownership/cancellation, brightness recovery, visible PPS off failure | Software gate passed: 61/61 desktop tests; focused final rerun; Tab5 build |
| 4 | Truthful RTC/SD/sensor results, restore RTC setting, fresh Home status | Software gate passed: 65/65 desktop tests; RTC fault journeys; Tab5 build |
| 5 | Wi-Fi restoration/recovery; verified HTTPS OTA, progress, cancellation, confirmation and recovery | Software gate passed: 72/72 desktop tests; OTA fault/cancel journeys; Tab5 build |
| 6 | Focused Home, exact PPS entry, populated logs/files/networks, keyboard/error/busy journeys | Software gate passed: 75 desktop checks (guard rename rerun); populated render/pointer journeys; Tab5 build |
| 7 | Build/test CI, separate smoke harness, extension contracts, device acceptance and release procedure | Software gate passed: fresh build, 76/76 tests, no-op rebuild, Tab5 build; hosted CI/device gates pending |

Feature parity checklist: launcher/categories; Network scan/select/connect/disconnect/forget/AP;
Updates install/cancel/reboot; brightness/volume; screen/motion/timed sleep/power-off;
RTC read/set; Power Monitor; IMU; switches; GPIO claims/releases; internal/external I2C scan;
SD listing; COM receive/transmit; camera start/stop; dual/headphone microphone and music; PPS
identity/readout/setpoints/presets/enable/priority off/communication-loss recovery.

## Reproduce software validation

```sh
python3 fetch_repos.py --verify
cmake --preset desktop
cmake --build --preset desktop -j 8
ctest --preset desktop --output-on-failure
# In an ESP-IDF v5.4.2 shell:
idf.py -C platforms/tab5 build
```

## Device release gate (not yet run)

The user confirmed on 2026-10-17 that no board or peripherals are connected to this laptop.
Device-only acceptance is blocked on that setup; no firmware has been flashed during stabilization.

- Record installed firmware identity and partition map; the baseline changes the NVS and app
  partition layout, so do not assume an OTA image can migrate the installed partition table.
- Record physical board/peripherals and available recovery connection before deployment.
- Measure input-to-visible feedback, navigation, render/flush time, lock waits, free/minimum heap
  and task stacks during PPS polling, camera/audio, scans and network operations.
- Initial targets for evaluation: p95 visible feedback below 100 ms; ordinary navigation below
  250 ms. Slow operations must show immediate busy feedback and leave Back/safety controls usable.
- Exercise absent peripherals, failed I2C writes/reads, failed camera stop, audio start/stop errors,
  repeated failed power attempts, brightness recovery and physical touch/motion/timed wake.
- Verify GPIO electrical states and PPS readback/off latency with controlled loads. Communication
  loss must remain Unknown; a software off request is not proof of physical output shutdown.
- Verify Wi-Fi across reboot and scans; OTA trust rejection, interruption/cancellation, boot/apply
  and recovery on the actual partition layout.
- Run repeated open/operate/close/reopen cycles and sustained operation; compare memory/tasks/timers
  after warm-up. Keep unverified features listed here until evidence exists.

## Milestone 2 evidence and execution contract

Verified 2026-10-17: desktop configure/build; 56/56 CTest tests including full app journeys
through the LVGL lifecycle timer; ESP-IDF v5.4.2 Tab5 build. Sensor controller regressions
also passed AddressSanitizer and network snapshot stress tests passed ThreadSanitizer.
Independent review found no blocker in lifecycle ownership, runner lifetime or shutdown ordering.

Mooncake lifecycle dispatch runs from one LVGL timer. Desktop SDL and LVGL share the main
thread; firmware uses the LVGL port task. Hardware acquisition is queued outside the display
lock and publishes copied snapshots. Views may close while samples are pending. Generic worker
admission stops and active jobs drain before HAL destruction; a timed-out drain refuses destruction.
Network strings and status are published under a short snapshot mutex, separate from SDK work.

Diagnostics records render time, HAL callers' lock wait/hold time, software input-to-flush-callback
latency, and device heap/task/UI-stack counters. This does not measure physical panel presentation
or all internal LVGL-port lock holds. Real device latency, memory and stack acceptance remain pending.

## Milestone 3 evidence and remaining hardware limits

Verified 2026-10-17: 61/61 desktop tests, followed by a passing five-test focused rerun after the
final audio volume/clock corrections; Tab5 firmware build. Production BMI wrapper tests passed
ASan/UBSan, audio session/volume tests passed TSan. Camera cleanup and codec-clock transaction
checks inject failures at the resource boundary. Rendered Home off-failure warning and Diagnostics
were inspected. No device was connected or flashed.

Camera start/stop uncertainty quarantines DMA resources until reboot; process teardown also drains
its independent capture task before HAL destruction. BMI initialization is idempotent and serialized,
and power rollback restores normal sampling while preserving the original error. Audio has one owner,
registered jobs, cancellable transfers, and verified software clock handoff between 44.1 kHz playback
and 48 kHz four-channel capture. Failed drains retain ownership; zero volume remains muted.

The current SDK has a one-second I2S transfer timeout and 100 ms codec-control I2C timeout. Its
mute/volume setters discard some underlying driver errors, so API return checks do not prove those
physical writes. Playback quality, physical cancellation latency, DMA faults, touch/motion/timed wake,
PPS output-off behaviour and restoration on the actual board remain device acceptance gates.

## Milestone 4 data contracts

Verified 2026-10-17: desktop build and 65/65 CTest tests; additional full UI runs with invalid RTC and
RTC/interface read failures; Tab5 firmware build. RTC calendar/controller and PM/IMU/SD/Power Settings
focused tests also passed sanitizer runs. Rendered RTC setting/confirmation and Home screens were inspected.
Independent ownership review found no blocking cache, RTC lock, SD session or teardown issue.

UI getters return copied snapshots, never hardware reads. PM, IMU, RTC and interface snapshots distinguish
the current attempt's validity from the presence/time of a previous successful sample. Failed transactions
retain that prior sample, and views mark failure/staleness rather than displaying unchecked zero values.
Home refreshes PM/RTC/interface data in a registered worker. CPU temperature has independent validity;
USB-A connection presence is explicitly unknown where the backend cannot establish it.

RTC reads check BCD fields, calendar/weekday consistency, oscillator validity and transaction results.
Normal writes preserve wake registers and verify the calendar; confirmed initialization is required after
invalid oscillator/stopped-clock state and clears wake configuration. The recovery register sequence follows
the [Epson RX8130CE manual](https://download.epsondevice.com/td/pdf/app/RX8130CE_en.pdf); its backup-charge
policy retains the board's original setting for the [Tab5 supercapacitor](https://docs.m5stack.com/en/core/Tab5).
Desktop clock changes affect only the simulator. SD scans own a serialized temporary mount, always attempt
cleanup after reads, distinguish empty results from failure, and cap results at 256 entries with a notice.

## Milestone 5 network and update contract

Verified 2026-10-17: desktop build and 72/72 CTest tests, rendered confirmation/progress/completion
screens, OTA transport/flash fault and cancellation regressions (including ASan), boot-health
ASan/UBSan tests, and the ESP-IDF v5.4.2 firmware build. No hardware was connected or flashed.

Network commands have one admission lease; OTA reserves the connection until transport/flash cleanup finishes.
Shutdown and real sleep/power preparation close admission and drain pending work. A failed power transition reopens
admission; Screen Off leaves networking active. Wi-Fi restores bounded NVS credentials without erasing unrelated NVS,
accepts 32-byte SSIDs, checks transition failures, and expires incomplete connection/DHCP attempts after 20 seconds.
Scans keep a healthy connection and verify its current link/address afterward. Stale events must match current intent
and the SDK's actual station/netif state before updating the cache.

OTA uses the installed ESP-IDF 5.4.2 `esp_http_client` streaming API with its certificate bundle and default hostname
verification, then `esp_ota_*` for the inactive partition. The high-level HTTPS OTA wrapper was unsuitable here because
it follows redirects internally. Redirects/non-200 responses are rejected. Transfers have a 5-second network timeout,
5-minute overall deadline, bounded 4 KiB buffer, partition/declared-length checks, chip/project header checks and SDK
whole-image validation. An atomic commit boundary closes cancellation immediately before selecting the boot image.
The app presents transferred bytes and explicit install/reboot confirmations; leaving the view does not cancel OTA.

The rebuilt bootloader enables ordinary OTA rollback (no anti-rollback eFuse changes). Pending images require a
minimum 3-second window of lifecycle/render progress and core display/input/registry availability, with a 10-second
health-check deadline. An independent timer armed before application initialization resets an unconfirmed boot after
15 seconds. Only a successful mark-valid operation and VALID readback disarm it. A timeout racing mark-valid may cause
an extra reboot; the SDK's redundant OTA metadata determines the selected image. These checks do not test peripherals.

**Deployment prerequisite:** the rollback-enabled bootloader and compatible partition table must be installed before
claiming rollback support. Existing-device partition/bootloader identity is unknown, so no app-only OTA migration has
been authorized or tested. Actual TLS interoperability, radio recovery, flash interruption and rollback remain device
acceptance gates. The SDK source used for this implementation is the pinned local v5.4.2 checkout, particularly
`esp_http_client`, `app_update/esp_ota_ops.c`, `bootloader_support`, and the system/timer startup code.

## Milestone 6 usability contract

Verified 2026-10-17: 74/75 tests passed in the full run, then the remaining source guard passed after
updating its reference to the renamed Home refresh function. Focused PPS, SD, COM and UART regressions
passed; UART/SD/COM also passed ASan. Rendered Home, PPS exact entry/editor, final network row, capped
SD listing and COM tail/reopen were inspected. LVGL pointer targeting confirms fixed critical controls
remain reachable. Final Tab5 firmware build passed; no hardware was connected or flashed.

Home uses the category rail and six readable shortcuts; diagnostics remain in Diagnostics. PPS has exact decimal
entry with strict limits, explicit Apply/Enable separation and a header Emergency Off available during editing.
LVGL target fields must not use `max_length` or `accepted_chars` with the PPS change callback: those options emit
per-character changes during `set_text`. Complete values are parsed atomically; parser length/range checks reject
invalid drafts. Saving an unchanged setpoint preserves its verified state.

SD results and COM logs use bounded viewports, retaining fixed actions. SD duplicate requests preserve an in-flight
result. COM retains 2,000 displayed bytes and follows the tail only when already there. UART worker storage is shared,
queues are bounded, driver operations run outside the UI queue mutex, and shutdown waits for driver cleanup. The
single worker owns UART SDK calls, including the SDK's internal TX mutex. Desktop echo creates no detached thread.
Network lists retain selection/scroll position during status refresh and support full 32-byte SSIDs in bounded rows.

## Milestone 7 reproducibility and handoff

Verified 2026-10-17: a fresh Ninja desktop build in `build/stabilization-clean`, 76/76 project CTest
checks, and a subsequent unchanged build reporting `ninja: no work to do`. The standard desktop
preset was also built. The firmware size check reports 15% free in the 4 MiB app slot and 5% free
in the configured bootloader region; future changes must keep both size checks passing. ESP-IDF v5.4.2 compiled the final app, bootloader and partition artifacts.
Eleven temporary-Git fetch cases run inside `fetch_repos_tests`; existing dependency pins verified
without network access. Workflow YAML and JSON configuration syntax were checked. The hosted
workflow has not been dispatched, and Linux/container execution remains a CI gate.

Desktop CMake no longer overwrites `CMAKE_BINARY_DIR`; binaries belong to the selected build tree.
VS Code run/debug uses `build/vscode-desktop/app_desktop_build`. Unused dependency demos/examples
and internal ThorVG are excluded from desktop builds. The old VS Code build cache repeatedly reported a truncated Ninja dependency log. It was preserved
as `build/vscode-desktop-before-stabilization-20261017` before recreating the standard preset build
directory. The recreated standard preset passed its no-op rebuild and full-app smoke journey too.
The cause was not conclusively explained; neither fresh build reproduced the damaged-log behavior. CI checks for unexpected compilation or damaged dependency logs on an
immediate rebuild.

The desktop journey harness is separate from runtime initialization, dispatch and shutdown. Independent review
compared its extracted functions and confirmed that the only intended dispatch change is the
injected pump of the runtime's existing LVGL timer. Runtime shutdown ownership remains unchanged.

New development should follow [the extension contract](EXTENDING.md). Use [the release checklist](RELEASE_CHECKLIST.md)
to record the actual board, recovery backup, partition/bootloader identity, controlled electrical
checks, radio/OTA interruption and rollback, wake behavior, visible latency and soak measurements.
Those device gates remain pending because no hardware is connected. No firmware has been flashed,
no remote CI run has been claimed, and no changes have been pushed during stabilization.
