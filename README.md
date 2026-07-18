# 123 Mobile Track — Tracker Firmware (LilyGo T-SIM7000G)

Production firmware for the in-vehicle GPS trackers behind
[123 Mobile Track](https://github.com/eamondoherty618-prog/123-mobile-track).
Current release: **v0.11.1** (see tags; `v0.9.48-known-good` is the fallback
baseline).

**Setting up new trackers?** See [DEPLOYMENT.md](DEPLOYMENT.md) — one generic
factory image, self-assigned identity from the chip MAC, Bluetooth adoption in
the app, and SD-card re-flash in the field.

## What it does

- **Telemetry over raw UDP through a 1NCE SIM** (fleet default since v0.10.1 —
  a fraction of the data cost of per-post TLS handshakes), received server-side
  via the 1NCE OS Cloud Integrator webhook. Out-of-order datagrams are dropped
  server-side.
- **HTTPS (modem-native `AT+SHREQ`) for what must be reliable**: motion
  transitions, OTA checks, heartbeats — with a raw-TLS fallback latch and a
  server-driven UDP-blackhole latch (`udp_age_s` in heartbeat responses).
- **GPS-motion ignition detection.** There is no vehicle-voltage sense: power
  is a 12V→5V buck and `AT+CBC` only reads the LiPo, so voltage-based ignition
  is impossible on this hardware. The LiPo also buffers the SIM7000G's ~2A
  bursts — a healthy battery matters; brownout is the prime reset suspect.
- **WiFi-scan geolocation fallback** when GNSS has no fix (async, un-hangable).
- **BLE identity + presence** (v0.10.9): the advertised name IS the identity
  (`123T-<id>`), so the app identifies trackers straight from the airwaves —
  no GATT connection (an iOS read-hang burned days before this design).
  Unadopted boards advertise continuously for in-app claiming with a 30s
  keep-alive (WiFi scans silently kill advertising) and evict any phone that
  holds a connection >45s (a leaked connection makes a tracker invisible).
  Adopted boards advertise only while driving with a fix — that powers the
  driver-in-vehicle alert — so the WiFi-scan fallback keeps working.
- **OTA updates** from the web app: chunked download, MD5 integrity,
  force-update support.
- **Self-healing diagnostics**: hardware watchdog, hang breadcrumbs
  (`last_hang_op` survives reset and is reported in telemetry), reset-reason
  reporting, bounded network waits. This is how the mid-drive freeze was
  cracked (v0.9.28–0.9.32).
- **Optional add-on hardware** (v0.11.x — runtime-detected, one firmware runs
  every combination): **CAN/OBD2** via TWAI on 32/33 (live RPM/speed/coolant/
  throttle, stored+pending DTCs, VIN via ISO-TP, server-commanded DTC clear;
  probes listen-only first so a live vehicle bus can never be disturbed) and
  an **ADXL375 high-g accelerometer** on I2C 21/22 (100 Hz impact/tamper
  detection in its own task). See DEPLOYMENT.md for the vehicle-safety
  checklist before tapping an OBD2 port.

## Layout

| Path | What it is |
|---|---|
| `src/`, `include/` | The firmware. Config in `include/tracker_config.h`. |
| `platformio.ini` | Build config (PlatformIO, ESP32). `factory` env = the generic new-board image; `bench_debug` adds raw CAN + accel serial dumps. |
| `src/can_obd2.*`, `src/accel_tamper.*` | Optional add-on hardware modules (headers in `include/`) — runtime-detected, dormant when absent. |
| `flash_new_tracker.sh` | Bench-flash a new board, print its identity, log to `fleet_inventory.csv`. |
| `make_sd_card.sh` | Write a microSD recovery/update card (field re-flash, no laptop). |
| `deploy_firmware.sh`, `package_firmware.sh` | Build + publish a release to the app's OTA store. |
| `fleet_inventory.csv` | Log of every flashed board (date, MAC, assigned ID). |

## Build, flash, release

```sh
pio run                    # build
pio run -t upload          # flash over USB
./deploy_firmware.sh       # package + publish as an OTA release
```

Note: the OTA publish writes to Netlify Blobs, so `deploy_firmware.sh` runs the
upload from the Netlify-linked frontend of the **app repo**
(`~/123-mobile-track/frontend`) — it handles that itself; just run it from here.
This repo is firmware only; the web app and serverless functions live in the
separate [`123-mobile-track`](https://github.com/eamondoherty618-prog/123-mobile-track) repo.

## Conventions

- Every release bumps the version in the firmware and lands as **one commit
  titled `vX.Y.Z — summary`**, tagged `vX.Y.Z`. Commit + push after every
  change (including experiments) so any unit in the field can be matched to
  exact source and rolled back.
- OTA rollout order: bench unit first, then the fleet.
