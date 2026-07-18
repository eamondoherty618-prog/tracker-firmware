# Hardware Wiring

This page is the quick reference for the tracker hardware that actually lives
in this repo.

## Core board

The main board is a `LilyGo T-SIM7000G`.

| Function | Pins |
|---|---|
| Modem UART | `GPIO 26` RX, `GPIO 27` TX |
| Modem power key | `GPIO 4` |
| microSD SPI | `GPIO 2` MISO, `GPIO 13` CS, `GPIO 14` SCLK, `GPIO 15` MOSI |
| CAN / OBD2 add-on | `GPIO 32` TX, `GPIO 33` RX |
| ADXL375 accelerometer | `GPIO 21` SDA, `GPIO 22` SCL, `GPIO 34` INT1 |

## CAN / OBD2 add-on

The CAN module is passive unless the firmware detects and uses it.

| CAN module terminal | Connect to |
|---|---|
| `H` | OBD2 pin `6` |
| `L` | OBD2 pin `14` |
| `G` | OBD2 pin `4` |
| `HV` | Leave empty |

| Grove lead | Connect to |
|---|---|
| yellow (CAN TX) | `GPIO 32` |
| white (CAN RX) | `GPIO 33` |
| black (GND) | GND |
| red (5V) | tracker `VIN 5V` |

Important:

- Do not power the CAN board from OBD2 pin `16`.
- Remove the `120 ohm` terminator from the CAN board.
- First live vehicle test should start in listen-only mode.
- Plug the unit in with ignition off.

## ADXL375 accelerometer

| Breakout pin | Connect to |
|---|---|
| `VCC` / `VS` | `3V3` |
| `GND` | GND |
| `CS` | `3V3` |
| `SDA` | `GPIO 21` |
| `SCL` | `GPIO 22` |
| `INT1` | `GPIO 34` reserved for wake in a future revision |

Important:

- This board is `3.3V` only.
- If the accelerometer is wired backwards from the silkscreen, trust the scan
  result in the bench debug build, not the label.

## Data flow

| Event | What happens |
|---|---|
| Tracker boots | Self-provisions an identity if needed, then starts telemetry. |
| Tracker is moving | GPS motion shortens the heartbeat interval. |
| Tracker is parked | Heartbeat slows down to save data and power. |
| OBD2 is present | Firmware polls RPM, speed, coolant, throttle, VIN, and DTCs. |
| Impact sensor is present | Firmware records high-g events and reports peaks. |

