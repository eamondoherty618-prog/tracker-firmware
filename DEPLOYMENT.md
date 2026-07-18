# Deploying Trackers

Use this guide when you are setting up a new tracker, recovering a tracker in
the field, or wiring optional add-on hardware.

For a printable start-to-finish shop guide with diagrams, start with
[Build one tracker](docs/build-one-tracker.md).

If you just want the wiring and pinout, start with
[Hardware wiring](docs/hardware-wiring.md). If you want the overall system
flow, start with [Project map](docs/project-map.md).

## 1. Bench flash a new board

Use the factory image for every brand-new board.

```sh
./flash_new_tracker.sh
```

What happens:

- The board gets the same generic factory firmware.
- It names itself from the last six hex digits of its efuse MAC.
- The ID is logged to `fleet_inventory.csv`.
- The tracker advertises over BLE so the mobile app can claim it.

After flashing:

- Unplug and replug the board once.
- Put the printed ID on the case.
- Insert the SIM and power it in the vehicle.

## 2. Adopt it in the app

In the mobile app:

1. Open `Devices`.
2. Tap `Add Device`.
3. Claim the tracker that matches the label on the case.
4. Assign it to a vehicle.

After adoption:

- The tracker stops advertising continuously.
- It only advertises while the vehicle is on a trip.
- The dashboard starts showing live state for that vehicle.

## 3. Recover a unit with microSD

If the board is already running firmware `>= v0.10.5`, use the recovery card.

```sh
./make_sd_card.sh /Volumes/YOURCARD
```

Then:

1. Eject the card.
2. Insert it into the tracker.
3. Power-cycle the tracker.

The tracker checks the card at boot, verifies the MD5, flashes the new image,
and reboots. Identity in NVS is preserved, so the unit keeps its existing ID.

## 4. Publish an OTA release

```sh
./deploy_firmware.sh
```

This builds the firmware, uploads the binary to the OTA store, and updates the
release metadata. Devices pick it up on the next check-in.

## 5. Optional add-on hardware

See [Hardware wiring](docs/hardware-wiring.md) for the exact pins and safety
notes.

Short version:

- CAN / OBD2 uses `GPIO 32` and `GPIO 33`.
- The accelerometer uses I2C on `GPIO 21` and `GPIO 22`.
- `GPIO 34` is reserved for the accelerometer interrupt line.

## 6. Batch checklist

- [ ] Boards, SIMs, cases, labels
- [ ] One factory flash per new board
- [ ] `fleet_inventory.csv` updated
- [ ] SIMs inserted and active
- [ ] Vehicle install verified on the map
- [ ] Optional CAN / accelerometer wiring checked against the wiring doc
