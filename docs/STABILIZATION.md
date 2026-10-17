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
| 3 | Camera failed-stop safety, BMI270 wake rollback, audio ownership/cancellation, brightness recovery, visible PPS off failure | Pending |
| 4 | Truthful RTC/SD/sensor results, restore RTC setting, fresh Home status | Pending |
| 5 | Wi-Fi restoration/recovery; verified HTTPS OTA, progress, cancellation, confirmation and recovery | Pending |
| 6 | Focused Home, exact PPS entry, populated logs/files/networks, keyboard/error/busy journeys | Pending |
| 7 | Build/test CI, separate smoke harness, extension contracts, device acceptance and release procedure | Pending |

Feature parity checklist: launcher/categories; Network scan/select/connect/disconnect/forget/AP;
Updates install/cancel/reboot; brightness/volume; screen/motion/timed sleep/power-off;
RTC read/set; Power Monitor; IMU; switches; GPIO claims/releases; internal/external I2C scan;
SD listing; COM receive/transmit; camera start/stop; dual/headphone microphone and music; PPS
identity/readout/setpoints/presets/enable/priority off/communication-loss recovery.

## Reproduce software validation

```sh
cmake --preset desktop
cmake --build --preset desktop -j 8
ctest --preset desktop --output-on-failure
# In an ESP-IDF v5.4.2 shell:
idf.py -C platforms/tab5 build
```

## Device release gate (not yet run)

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
