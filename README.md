# LilyGo T-SIM7000G Fleet Tracker Prototype

This is a first-pass cellular fleet tracker firmware for a LilyGo T-SIM7000G.
It reads GNSS data from the SIM7000G modem, posts JSON telemetry to a server,
and detects basic driving events from GPS speed changes.

## What It Reports

- Location, speed, altitude, and satellite count when GNSS has a fix
- Moving/parked state
- Hard braking and rapid acceleration events from speed deltas
- Cellular signal quality
- Battery voltage reported by the modem
- Firmware version, uptime, and queued message count

## Configure It

Edit `include/tracker_config.h`:

- `TRACKER_APN`
- `TRACKER_SERVER_HOST`
- `TRACKER_SERVER_PORT`
- `TRACKER_SERVER_PATH`
- `TRACKER_DEVICE_ID`
- `TRACKER_AUTH_TOKEN`

Start with HTTP for bench testing. Use TLS, real per-device credentials, and a
signed OTA path before installing this in real vehicles.

## Build And Upload

Install PlatformIO, then run:

```sh
pio run -t upload
pio device monitor
```

The project is already set to use the detected serial port:

```text
/dev/cu.usbserial-59680313751
```

## Server Contract

The firmware sends `POST /api/fleet/telemetry` with JSON:

```json
{
  "device_id": "tracker-001",
  "uptime_ms": 123456,
  "firmware": "0.1.0",
  "has_fix": true,
  "motion_state": "moving",
  "event": "hard_brake",
  "cell_rssi": 21,
  "battery_mv": 4100,
  "queued_messages": 0,
  "gps": {
    "lat": 40.7128,
    "lon": -74.006,
    "speed_kph": 52.1,
    "altitude_m": 12.3,
    "satellites": 9
  }
}
```

## Run The Prototype Server

This repo includes a no-dependency local receiver:

```sh
python3 server/fleet_server.py --port 8080
```

Open:

```text
http://localhost:8080
```

Test it from the laptop:

```sh
curl -X POST http://localhost:8080/api/fleet/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":"tracker-001","motion_state":"moving","gps":{"lat":40.7128,"lon":-74.0060,"speed_kph":33.2}}'
```

Packets are appended to:

```text
data/telemetry.jsonl
```

For the cellular board to reach this server from a SIM card, the server needs a
public URL. A local laptop URL like `localhost` or `192.168.x.x` will not be
reachable over the cellular network unless you use a tunnel or deploy the API.

## Prototype Notes

GPS speed is useful but not ideal for harsh-driving detection. Add an IMU for
better hard braking, cornering, crash, tow, and vibration detection.

For vehicle installation, add:

- Fused 12 V to 5 V power conversion
- Ignition or accessory-line sensing
- Low-voltage car battery protection
- Tamper or unplug detection
- Store-and-forward persistence in flash, not only RAM
