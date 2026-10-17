# M5Tab5 User Demo

User demo source code of [M5Tab5](https://docs.m5stack.com/en/products/sku/k145).

## Build

### Fetch Dependencies

```bash
python ./fetch_repos.py
```

## Desktop Build

#### Tool Chains

```bash
sudo apt install build-essential cmake libsdl2-dev
```

#### Build

```bash
mkdir build && cd build
```

```bash
cmake .. && make -j8
```

#### Run

```bash
./desktop/app_desktop_build
```

## IDF Build

#### Tool Chains

[ESP-IDF v5.4.2](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/index.html)

#### Build

```bash
cd platforms/tab5
```

```bash
idf.py build
```

#### Flash

```bash
idf.py flash
```

## App Architecture

The UI is registry-driven. New features should be added as standalone Mooncake apps and registered in
`app/apps/app_catalog.cpp` with an `AppKey`, category, label, icon, summary priority, and status provider.

App screens should use the shared shell in `app/apps/ui/`:

- Create the full-screen root through `app_ui::AppShell`.
- Put all controls under `AppShell::content()`, which is the safe 1232x600 content area.
- Use the shared UI primitives for panels, buttons, metric cards, chips, and empty states.
- Do not create app controls directly on `lv_screen_active()`; use the shared shell. Global toasts are separate overlays.
- Bind single-line text fields with `AppShell::bindTextInput()` for the shared touchscreen editor with save/cancel.
- Keep hardware calls behind app-owned views/controllers and use the HAL rather than embedding launcher-specific logic.

### Add A New App

```bash
python app/apps/app_generator.py "Fan Control"
```

The generator creates an `AppShell`-based app folder and prints the catalog changes to make. Add the new `AppKey` in
`app/apps/app_registry.h`, include the new app header in `app/apps/app_catalog.cpp`, and add one `apps.add(...)` entry in
`app_catalog::registerDefaultApps()`.

After adding the catalog entry, re-run CMake so new source files are picked up:

```bash
cmake --preset desktop
cmake --build --preset desktop --target app_desktop_build
ctest --preset desktop --output-on-failure
```

The desktop UI smoke tests exercise network selection, connection, rescanning, disconnect, and manual AP control.
Set `M5TAB5_DESKTOP_WIFI_NO_NETWORKS=1` for empty scan results or `M5TAB5_DESKTOP_WIFI_CONNECT_FAIL=1` for connection
failure. These simulator flags do not affect Tab5 builds.

Wi-Fi restores validated saved credentials at startup, supports a full 32-byte SSID, and preserves an established
connection while scanning. Failed transitions and connection/DHCP timeouts are visible. Network changes run outside
the UI and are unavailable while an update owns the connection.

Updates accepts HTTPS URLs only, verifies the server certificate/hostname using the bundled trust store, rejects
redirects, and checks the image's chip, project, size and SDK image validation before selecting it for boot. A valid
RTC/system time is needed for certificate checks. Install and reboot each require confirmation. Progress uses actual
bytes; unknown-length transfers show no invented percentage. Cancel works until final boot selection; Back leaves
the transfer running and reopening Updates restores its status. Desktop uses a simulated image and never flashes or
reboots the host; `M5TAB5_DESKTOP_OTA_FAIL=1` and `M5TAB5_DESKTOP_OTA_UNKNOWN_LENGTH=1` exercise these cases.

Rollback is enabled in the build. A pending image must demonstrate core UI/input setup and sustained lifecycle/render
progress before it is marked valid. An independent 15-second startup watchdog covers initialization stalls. This is a
startup check, not peripheral acceptance. **Rollback requires the matching rebuilt bootloader to be flashed.** Updating
an app image alone cannot install that bootloader or migrate a partition table. Follow the
[device release gate](docs/STABILIZATION.md) before deploying to existing hardware.

Camera capture reports Starting/Running/Stopping/Failed states from the HAL rather than assuming startup succeeded.
`stopCameraCapture()` detaches the LVGL target before returning; app views can then be destroyed while hardware
cleanup finishes. A new capture cannot start until that cleanup completes. Tab5 previews use separate render/display
buffers, preserve the source aspect ratio, and fail visibly if no frame arrives within two seconds.
If capture start/stop cannot prove DMA has stopped, its driver resources are retained and camera restart is disabled
until reboot. Failed scaler release similarly retains its preview buffers. Process teardown waits outside the display
lock for capture cleanup; a timeout prevents HAL destruction.

Desktop camera smoke tests check animated preview pixels, repeated start/stop, closing during capture and before the
first frame, resource cleanup, and recovery from faults. Set `M5TAB5_DESKTOP_CAMERA_FAIL` to `start`, `frame`, or `stall`
to exercise those failure states. These checks do not replace repeated capture/close and missing-camera tests on Tab5.

PPS uses a serialized background worker and synchronized UI snapshots. First connection and reconnection verify
module identity and request output-off before controls unlock. Unknown/stale output state is never displayed as off.
Apply requires confirmed output-off and verifies both setpoint registers; Enable remains locked after partial or missed
writes until a successful reapply. Emergency Off takes priority over queued commands and interrupts polling between
register transactions. Back cancels queued Apply/Enable and requests output-off asynchronously for an identified PPS; communication failure
can still leave the physical output live, so this is not a substitute for a hardware disconnect.
Home retains pending/off-unconfirmed warnings after Back. Reopening PPS permits an off retry; only successful
hardware readback clears the off failure. Successful old readback still becomes Unknown when telemetry is stale.

GPIO diagnostics are opt-in: opening the app never configures or drives a pin. Drive Low claims one permitted
expansion pin before its level toggle unlocks. Release, Release All, and Back return only this app's claimed pins
to input mode with no pull resistors. Port A I2C pins are protected, internal peripheral pins are absent from the
inventory, and Tab5 UART console pins are reserved when that console is configured. Legacy pins 0/1 remain
protected until their expansion role is verified for this board. The HAL rejects unknown,
protected, or already claimed pins before changing hardware.

Failed initialization, writes, and releases show Unknown rather than claiming a known output level. Release remains
available for partially configured pins; Back stays open when cleanup fails so it can be retried. The GPIO grid
scrolls independently of Back and Release All. Desktop UI tests exercise explicit claims, protection, an independently
claimed output, lower-row scrolling, cleanup, and failure recovery. Set `M5TAB5_DESKTOP_GPIO_FAIL` to `init`, `set`, or
`release` for faults. Hardware acceptance still requires checking pin levels/impedance and unaffected I2C operation
on Tab5. A displayed output level is the last successful command, not an electrical measurement.

Power Settings requires confirmation before screen-off, motion sleep, timed sleep, or shutdown. Cancel and Back
dismiss an unconfirmed request without calling hardware. Preparation runs in a serialized background worker;
reopening the app reflects the current operation rather than starting another one. Power telemetry uses synchronized
cached snapshots so a UI read does not wait for an in-flight I2C transaction.

Screen Off turns off the backlight only; apps and networking remain active. Tab5 filters its existing touch input
stream to wake the display after a fresh press and consumes that gesture until release, preventing click-through.
There is no second touch-reader task or indefinite LVGL lock. Timed Sleep accepts 1-60 minutes and configures the
RX8130CE relative wake timer with readback verification; it never changes clock/calendar registers. Motion wake
disable is also verified before shutdown preparation.
Ordinary brightness adjustment is limited to 5-100%; zero is reserved for Screen Off. BMI270 initialization reuses
one device handle and serializes driver access. Returned power failures attempt RTC cancellation and restore normal
IMU sampling, reporting rollback failures alongside the original error.

Tones, music and microphone tests share one codec owner. Stop/Back requests cancellation without waiting on the UI;
errors are visible and task-start failures release the reservation. Audio transfers check driver results in chunks;
the installed Tab5 codec driver has a one-second IO timeout. Power preparation stops all audio with a two-second
deadline; a timeout retains ownership until real completion. The MP3 player remains allocated for the process lifetime
because its vendor delete path does not guarantee its task has stopped before freeing memory.
Playback and capture reconfigure both codec directions together for the bundled 44.1 kHz music and 48 kHz
four-channel recording. Zero volume remains muted across those transitions. The installed codec library discards
some low-level mute/volume write errors, so successful API returns alone do not verify those physical register writes.
If the power-off signal returns and the device is still running after 1.5 seconds,
the controller reports failure, disarms wake sources where possible, and restores the display. External PPS/module
outputs can remain live independently; confirmation warns about that and does not silently change those outputs.
Desktop simulates these actions without powering off the host. Set `M5TAB5_DESKTOP_POWER_FAIL=1` for a recoverable
action failure. Tests cover confirmation/cancel, background dispatch, app close/reopen, screen-off/wake, failure recovery,
timer register faults, and control overlap. Physical touch wake, motion wake, timed wake, and failed-shutdown cleanup
still require acceptance on Tab5.

Desktop PPS tests cover startup, communication loss, reconnection, partial writes, readback mismatch, concurrent edits,
Emergency Off during a blocked poll, and closing during a queued command. `M5TAB5_DESKTOP_PPS_ABSENT=1` exercises
the missing-device UI. Smoke journeys also inject a failed off write and verify the warning survives on Home.
Actual off latency, load behaviour, and failed-communication safety still require Tab5/PPS tests.

RTC, power, IMU, and interface status use cached snapshots with validity and sample age. Failed reads retain the
last successful values and identify them as unavailable/stale; a checked zero remains a valid measurement. Desktop
samples are labelled simulated. Home refreshes these snapshots in a worker and never performs I2C reads while drawing.
RTC offers a confirmed date/time editor for 2000-2099 and verifies the written calendar. Normal setting preserves
wake configuration. An invalid oscillator/stopped clock requires explicit initialization, which clears wake settings;
startup never silently clears the invalid-clock flag. Desktop RTC setting does not change the host clock.

SD scanning reports mount/read/cleanup errors separately from an empty directory, unmounts after each scan, and
limits results to 256 entries with a visible truncation notice. These software checks do not establish physical card,
RTC backup, sensor accuracy, or board-connection behavior; see [stabilization and acceptance](docs/STABILIZATION.md).

PPS accepts decimal voltage/current entry up to three places (0.500-30.000 V and 0.000-5.000 A).
This is input precision, not a claim of physical output accuracy. Editing stages values; Apply writes and verifies
setpoints while output is off, and Enable is a separate action. Invalid input blocks Apply/Enable. Emergency Off
remains in the header during keyboard editing and discards the draft before requesting shutdown.

Home presents six shortcuts without a duplicate summary sidebar. SD results, network scans and COM logs scroll
inside their own panels while Back and primary actions stay fixed. COM keeps the last 2,000 displayed bytes and
follows new data only while viewing the end. UART receive/transmit queues are bounded; rejected messages are
reported, and serial workers finish driver cleanup before HAL destruction. Desktop serial data is simulated echo.


## Acknowledgments

This project references the following open-source libraries and resources:

- https://github.com/lvgl/lvgl
- https://www.heroui.com
- https://github.com/Forairaaaaa/smooth_ui_toolkit
- https://github.com/Forairaaaaa/mooncake
- https://github.com/Forairaaaaa/mooncake_log
- https://github.com/alexreinert/piVCCU/blob/master/kernel/rtc-rx8130.c
- https://components.espressif.com/components/espressif/esp_cam_sensor
- https://components.espressif.com/components/espressif/esp_ipa
- https://components.espressif.com/components/espressif/esp_sccb_intf
- https://components.espressif.com/components/espressif/esp_video
- https://components.espressif.com/components/espressif/esp_lvgl_port
- https://github.com/jarzebski/Arduino-INA226
- https://github.com/boschsensortec/BMI270_SensorAPI
