# Deploying trackers to the field

The pipeline (firmware ≥ v0.10.5): every board gets the **same generic factory
image** — no per-device compiling, no config editing. Identity and adoption are
handled automatically.

```
 bench: USB flash (once)          vehicle: power it            app: adopt it
┌─────────────────────────┐   ┌───────────────────────┐   ┌─────────────────────────┐
│ ./flash_new_tracker.sh  │   │ tracker self-names as │   │ Devices → Add Device →  │
│ prints "tracker-a1b2c3" │ → │ tracker-<mac6>, gets  │ → │ claim it → assign       │
│ + logs to inventory CSV │   │ online, advertises BLE│   │ vehicle → on the map    │
└─────────────────────────┘   └───────────────────────┘   └─────────────────────────┘
```

## 1. Bench: initial flash (USB, once per board — unavoidable)

A blank ESP32 has no SD/USB-drive bootloader in silicon, so the very first
flash must be over the USB port. After that, the unit never needs a cable again
(OTA + SD card cover everything).

Per board:

1. Plug the board into USB (one at a time keeps port detection unambiguous).
2. `./flash_new_tracker.sh`
3. The script prints the unit's permanent identity — `tracker-<last 6 of MAC>`
   — and appends it to `fleet_inventory.csv`. **Write the ID on the case**
   (label maker / sharpie); it's how the installer finds it in the app.
4. Insert the 1NCE SIM. In the 1NCE portal, if the SIM had an old data session
   from before 1NCE OS activation, hit **Reset connection** so UDP routes.

No SIM yet? Fine — flash anyway; identity comes from the chip, not the SIM.

After flashing, **unplug and replug the board once** (power-cycle): the first
boot straight out of the flasher occasionally wedges the BLE radio; a clean
power-up always advertises, and the 30s keep-alive holds it visible.

## 2. Vehicle: install

- Wire the 12V→5V buck to switched or constant 12V, board in its case, GPS
  antenna with sky view, LiPo connected (it buffers the modem's ~2A bursts —
  a missing/dead LiPo is the #1 cause of brownout resets).
- Power it up. Within a couple of minutes it's posting telemetry under its ID,
  and it advertises over Bluetooth **continuously** until someone adopts it.

## 3. App: adopt (any phone with the iOS app, standing near the vehicle)

1. Devices → **Add Device** → the tracker appears by its ID (matches the label).
2. Claim it and assign/create its vehicle.
3. Within ~15 minutes (next heartbeat) the tracker learns it's adopted and
   stops its setup advertising for good — from then on it only advertises
   during trips, which is what powers driver-presence alerts.
4. Verify it shows on the live map, then done.

Un-claiming a device in the app reverses this: the tracker reverts to
continuously advertising, ready to be adopted by another account.

## Field re-flash / recovery: the SD card (no laptop needed)

Any unit running ≥ v0.10.5 checks its microSD slot at boot:

- **Make the card**: `./make_sd_card.sh /Volumes/YOURCARD` (FAT32 card; writes
  `firmware.bin` + `firmware.md5`).
- **Use it**: insert card, power-cycle the tracker. It verifies the MD5,
  flashes, and reboots into the new firmware. Identity in NVS is untouched —
  the unit stays `tracker-xxxxxx`, still adopted.
- The card can stay inserted: an already-applied image is skipped (MD5 marker
  in NVS), so it won't re-flash in a loop. Carry one card in the truck as a
  recovery stick.
- Corrupt copy (md5 mismatch) = it refuses to flash and boots normally.

## Routine updates: OTA (no touching the vehicle)

For units already in the field and healthy, keep using the normal path:
`./deploy_firmware.sh` publishes a release; devices are offered it on their
next heartbeat and can be force-updated from the Devices page.

**Rollout order, always: bench unit first, then one vehicle, then the fleet.**

## Batch-day checklist (N boards)

- [ ] Boards, SIMs, cases, labels, one FAT32 microSD (recovery card)
- [ ] `./flash_new_tracker.sh` per board; label each with its printed ID
- [ ] SIMs inserted; 1NCE portal: all SIMs enabled, sessions reset if stale
- [ ] `fleet_inventory.csv` committed after the batch
- [ ] Install + adopt per vehicle; live map check per vehicle

## Add-on hardware: CAN/OBD2 tap + impact sensor (fw ≥ v0.11.x)

Optional per-vehicle extras — the same firmware detects them at runtime, and a
tracker without them behaves exactly as before.

### Wiring (as-built, bench-verified 2026-07)

**Mini CAN (U179, TJA1051T) — screw terminals ← OBD2 pass-through cable:**

| Terminal | OBD2 pin | Cable wire color |
|---|---|---|
| H | 6 (CAN-H) | solid red |
| L | 14 (CAN-L) | yellow |
| G | 4 (chassis ground) | green |
| HV | **EMPTY — never wire pin 16 here** | orange-white stays taped |

Cable bundle traps: solid orange = pin 3 and red-white = pin 15 — never
confuse them with the pin 6 / pin 16 wires.

**Mini CAN Grove leads → LilyGo:**

| Grove lead | LilyGo |
|---|---|
| yellow (CAN TX) | GPIO 32 |
| white (CAN RX) | GPIO 33 |
| black (GND) | GND |
| red (5V) | **VIN 5V pin** — the unit powers FROM the tracker |

⚠️ **The unit must never be powered from OBD2 pin 16.** A transceiver that
wakes up while the ESP32 driving its TXD is dead gets TXD dragged low
through the dead chip's clamp diodes and **jams the bus dominant** the
instant the connector seats — this caused three rounds of instant
StabiliTrak / power-steering dash warnings before being traced. Powering
from the tracker's rail makes the failure state physically impossible.
The unit's 120Ω terminator (SMD marked `121`) is removed — see checklist.

**ADXL375 accelerometer → LilyGo (3.3V ONLY — no 5V tolerance):**

| Breakout pin | Connect to |
|---|---|
| VS + VCC (tied) | 3V3 |
| GND + SDO (tied) | GND |
| CS | 3V3 (firm joint — floating/cold = SPI mode = mute on I2C) |
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| INT1 / INT2 | unconnected (INT1 reserved for GPIO 34 wake) |

⚠️ **This generic breakout's SDA/SCL silkscreen is unreliable** — as built,
the working wiring is *swapped relative to the labels*. Verify with the
bench_debug build's boot output: `I2C scan: 0x53` = correct; an empty scan
with both lines reading HIGH = swap the two data wires at the LilyGo.

### ⚠️ Vehicle-safety checklist — read before tapping an OBD2 port

Hard-won (first attempt bumped a truck's idle and latched power-steering /
StabiliTrak warnings):

1. **Remove the 120Ω terminator from the CAN module.** Hobby transceiver
   boards ship terminated; a vehicle bus already has its two terminators, and
   a third passively drags the whole network down — powered or not. Look for a
   TERM switch/jumper, else desolder the SMD resistor marked `121`. Verify
   with a meter: CANH↔CANL at the module must read OPEN.
2. **Verify the OBD2 pins with the mirroring trap in mind.** Numbering is
   defined looking INTO the vehicle's socket (1–8 top, 9–16 bottom). A male
   plug wired while viewed from the back is left-right mirrored — "6/14"
   counted wrong lands on 3/11, and on GM trucks pin 1 is single-wire GMLAN
   (chassis modules — instant StabiliTrak complaints). Meter checks at the
   plug: 6↔14 open, each to ground (4/5) open, each to battery (16) open.
3. **Power the CAN unit from the tracker's rail (Grove red → VIN 5V), never
   from OBD pin 16** — see the wiring section for the failure mode.
4. **Power the tracker before plugging into the OBD port**, and plug in with
   the **ignition OFF** (GM stability systems dislike hot-plug transients).
5. Keep the OBD-to-module wire short (under ~30 cm).
6. Meter pre-flight at the male plug: continuity 6→H / 14→L / 4→G, then
   6↔14, CAN↔ground, CAN↔pin-16 all OPEN.
7. First live session: flash the `bench_listen` env (CAN locked passive —
   cannot transmit); confirm `listen_frames` climbing in telemetry with a
   calm dash BEFORE flashing the normal build.
8. Staged test: ignition ON engine OFF → dash quiet 30s → engine on → quiet →
   done. Any warning: unplug (effects are transient), reassess.

The firmware's own guard: the CAN probe starts in TWAI listen-only mode
(physically cannot transmit, not even ACK bits) and only speaks after hearing
20+ healthy frames at 500k — so a silent, wrong-bitrate, or mis-wired bus is
never disturbed by us.

### Bench verification

Flash the `bench_debug` env — it prints an I2C scan + line check for the
accelerometer at boot, live g readings, raw CAN frames, and every probe state
change. The server payload gains optional `obd` and `impact` sections
(presence flags on HTTPS posts, live values on UDP), and stored DTCs can be
cleared remotely via `POST /api/fleet/obd/clear-dtc` (owner/admin only).
