# 123 Mobile Track

Firmware, recovery tools, and deployment helpers for the tracker hardware
behind `123 Mobile Track`.

Current firmware: `v0.11.1`

## Start here

- [Build one tracker](docs/build-one-tracker.md)
- [Project map](docs/project-map.md)
- [Hardware wiring](docs/hardware-wiring.md)
- [Deployment guide](DEPLOYMENT.md)

## What is in this repo

| Path | What it is |
|---|---|
| `src/`, `include/`, `platformio.ini` | Tracker firmware. |
| `flash_new_tracker.sh` | Flash a brand-new board over USB and log its ID. |
| `make_sd_card.sh` | Build a field-recovery microSD card. |
| `deploy_firmware.sh` | Build and publish an OTA firmware release. |
| `fleet_inventory.csv` | Flash log for boards that have been provisioned. |
| `hardware/obd_tracker_carrier_v1/` | KiCad carrier-board files, BOM, placement file, and mechanical references. |
| `docs/` | Plain-English build guide, diagrams, wiring, and system overview. |

## Quick build and flash

```sh
pio run
pio run -t upload
./flash_new_tracker.sh
./make_sd_card.sh /Volumes/YOURCARD
```

## How the system fits together

1. The tracker boots on a `LilyGo T-SIM7000G`.
2. It reads GPS and optional add-ons like CAN/OBD2 or the impact sensor.
3. It sends telemetry to `123mobiletrack.com`.
4. The app and dashboard show the live vehicle state.
5. OTA releases are published from this repo and pulled by the trackers on
   their next check-in.

If you need the wiring details or the install order, start with the build guide
above.

## Repo hygiene

Tracked files should be firmware, deployment scripts, docs, and hardware source
files. Generated outputs, local KiCad session files, zip packages, telemetry
logs, PlatformIO builds, and unrelated shop/design files are ignored.
