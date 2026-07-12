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
