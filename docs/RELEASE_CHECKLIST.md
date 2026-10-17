# Tab5 release and physical acceptance

**Current disposition: software validation only. No board or peripherals were connected on
2026-10-17, and stabilization has not flashed a device. Every physical check below is pending.**

Use one copy of this record for a specific board and candidate commit. Mark a check passed only with
its observed result, date and logs/measurements. A simulator screenshot is not hardware evidence.

## Candidate and recovery record

| Item | Record |
| --- | --- |
| Candidate commit / binary SHA-256 | Pending |
| Board identity / hardware revision | Pending |
| Existing firmware / bootloader identity | Pending |
| Installed partition table / flash size | Pending |
| Recovery USB/serial connection | Pending |
| Recoverable original flash/configuration backup | Pending |
| PPS, input supply, load, SD, headset and other peripherals | Pending |
| Operator / date / evidence location | Pending |

Before deployment, inspect the existing partition layout and bootloader. Save a recoverable backup
using the correct board/flash parameters and retain any credential-bearing configuration privately,
outside Git. Confirm the recovery connection works before replacing firmware. Preserve the baseline
tag `baseline/tab5-before-stabilization-20261017` and the tested candidate artifacts.

The candidate enables rollback in its **bootloader** and uses the repository partition table. An
app-only OTA cannot replace an incompatible installed partition table or install that bootloader.
Choose the initial provisioning method only after comparing the actual device with the candidate.
Do not claim rollback support from the app build alone. No anti-rollback eFuse change is part of this work.

## Software candidate gate

- [ ] Dependency verification passes with exact revisions and a clean dependency tree.
- [ ] Fresh desktop configure/build, complete project CTest suite and immediate no-op rebuild pass.
- [ ] Tab5 build passes with ESP-IDF v5.4.2; record app/bootloader/partition hashes and size reports.
- [ ] Rendered Home, Settings, Network, PPS keyboard/output, error and populated SD/COM states inspected.
- [ ] CI has run successfully for the candidate commit. Local checks do not establish hosted CI success.
- [ ] Candidate changes and outstanding physical limits reviewed; no unrelated work is included.

## Controlled physical commissioning

Start with PPS output unloaded and a suitable current-limited input supply. Confirm pin/module
assignments and recovery access before connecting loads. Add peripherals and load progressively.

- [ ] Boot/navigation: cold boot, rapid open/Back/reopen, all registered apps, populated scrolling and
  keyboard Save/Cancel/Back. Essential controls stay reachable and no operations remain stuck.
- [ ] PPS: correct identity and limits, exact setpoint readback, Apply while Off, separate Enable,
  Emergency Off during polling/editing, Back with output On, disconnect/reconnect and failed read/write.
  Measure actual off latency and output with an instrument; unknown/unconfirmed states stay visible.
- [ ] GPIO/power rails: only explicit owned pins change; protected/independently owned pins remain
  untouched. Measure electrical state and check failed release/readback reporting.
- [ ] Sensors/RTC/SD: absent and failed transactions do not appear as valid zeros; recovery refreshes
  timestamps. Test RTC read/set, invalid-clock initialization, backup retention, real SD mount/read/
  cleanup and a large listing. Confirm the actual board's RTC backup-charge arrangement.
- [ ] Audio/camera/serial: repeat start/stop/Back, cancel audio during transfers, switch audio modes,
  camera stop failure and repeated capture, RS485 send/receive and shutdown. Check for resource growth,
  corrupted audio/frames, blocked touch or reused resources after uncertain cleanup.
- [ ] Display/sleep: ordinary brightness cannot blank the screen; Screen Off wakes by touch. Verify
  motion/timed wake and failed power-transition recovery. External PPS outputs require their own checks.

## Radio, OTA and recovery

- [ ] Wi-Fi credentials survive reboot, full-length SSIDs work, scans preserve a connected link, and
  deliberate disconnect, lost IP, timeout and reconnect show the actual state. Failed persistence is visible.
- [ ] HTTPS rejects invalid certificates/hostnames, redirects, malformed/incompatible images and truncated
  bodies. Verify device clock/trust behavior with the intended update server.
- [ ] Progress reflects received bytes, including unknown-length responses. Cancel before commit stops
  transfer/flash and leaves the old boot selection; after the commit boundary cancellation is unavailable.
- [ ] Leave/reopen Updates during transfer, attempt other network commands, and exercise power preparation.
  The owner remains consistent and no UI path claims completion before cleanup.
- [ ] On the verified rollback-enabled bootloader/partition layout, interrupt download/write and first boot
  at controlled stages. Confirm recovery to a usable image and recoverable metadata. Verify pending-image
  health/watchdog failure and successful mark-valid behavior. Record the actual installed bootloader.
- [ ] Reboot confirmation makes externally powered outputs explicit; measure PPS state across device reboot.

## Responsiveness and soak

Record a warm baseline, then repeated app transitions and an extended mixed workload with PPS polling,
network/SD scans, camera and audio. Record duration, iteration count, minimum/free heap, task count,
stack high-water marks, render/flush and lock timing. Compare after warm-up for progressive growth.

Initial evaluation targets are p95 **visible** touch feedback below 100 ms, ordinary navigation below
250 ms and at least 30 FPS while scrolling. Record actual results and revise targets only with measured
evidence. Diagnostics measures software input-to-flush-callback latency, not physical panel presentation;
use an external observation/measurement for the visible-feedback claim.

Release only after the applicable physical checks pass. If any remain unverified, list those features
and limitations with the candidate instead of describing it as production-ready.
