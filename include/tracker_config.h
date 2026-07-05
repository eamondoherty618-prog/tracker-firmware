#pragma once

// Cellular SIM settings. Many IoT SIMs use an empty user/password.
#define TRACKER_APN_PRIMARY "iot.1nce.net"
#define TRACKER_APN_FALLBACK "iot.1nce.net"
#define TRACKER_GPRS_USER ""
#define TRACKER_GPRS_PASS ""

#define TRACKER_SERVER_HOST "123mobiletrack.com"
#define TRACKER_SERVER_HOST_HEADER "123mobiletrack.com"
#define TRACKER_SERVER_PORT 443
#define TRACKER_SERVER_PATH "/api/fleet/telemetry"
#define TRACKER_SERVER_URL "https://123mobiletrack.com/api/fleet/telemetry?k=c5cc56a23546eb487223fe810ae8a8b76d83376ee09140f7"

// These can be overridden via build flags (-D) in platformio.ini per environment.
// New boards use "unprovisioned"; they receive real credentials during WiFi setup.
#ifndef TRACKER_DEVICE_ID
#define TRACKER_DEVICE_ID "tracker-002"
#endif
#ifndef TRACKER_API_KEY
#define TRACKER_API_KEY "c5cc56a23546eb487223fe810ae8a8b76d83376ee09140f7"
#endif
#define TRACKER_FIRMWARE_VERSION "0.10.0"

// VESTIGIAL: alternator-voltage ignition detection. This hardware uses a 12V→5V buck
// with no voltage sense, and AT+CBC only reads the buffer LiPo (~3.8V), so these can
// never be reached. Trip state is now derived from GPS motion (see TRACKER_TRIP_GRACE_MS).
// Kept only for reference; safe to delete.
#define TRACKER_IGNITION_ON_MV  13200U
#define TRACKER_IGNITION_OFF_MV 12800U

// Tuning knobs.
#define TRACKER_MOVING_INTERVAL_MS 10000UL
#define TRACKER_PARKED_INTERVAL_MS 120000UL
#define TRACKER_MIN_MOVING_SPEED_KPH 3.0f
// How long after the last GPS motion we still treat the vehicle as "on a trip", so a
// red light / stop-and-go doesn't immediately collapse to the slow parked cadence.
#define TRACKER_TRIP_GRACE_MS 180000UL
// Harsh-event detection from GPS speed. The OLD logic flagged any absolute speed
// change over a window up to EVENT_WINDOW_MS, so a gentle 22 kph slowdown spread
// over 12 s tripped the SAME alert as a genuinely abrupt one — far too many false
// "hard brake" alerts. Now we threshold on the deceleration RATE (kph per second,
// normalised by the actual sample gap) and also require a minimum absolute change
// so one noisy GPS sample can't trip it. NOTE: with GPS speed only sampled every
// ~10 s these are still coarse; true harshness needs an IMU or high-rate sampling.
#define TRACKER_HARD_BRAKE_RATE_KPH_S  -4.0f    // <= this rate (kph/s) = hard brake
#define TRACKER_RAPID_ACCEL_RATE_KPH_S  5.0f    // >= this rate (kph/s) = rapid accel
#define TRACKER_EVENT_MIN_DELTA_KPH     12.0f   // ignore speed changes smaller than this
#define TRACKER_EVENT_MIN_DT_MS         2000UL  // gaps shorter than this = GPS jitter
#define TRACKER_EVENT_WINDOW_MS         12000UL // gaps longer than this aren't one event
// TLS connect timeout (seconds). TinyGSM defaults to 75s with no override, so a wedged
// SIM7000G CAOPEN can stall the whole loop long enough to hit the 180s loop watchdog
// (field freeze: last_hang_op="tlsConnect"). Bound it short so a bad connect fails fast
// and hands off to the escalating post-fail recovery instead of hanging.
#define TRACKER_TLS_CONNECT_TIMEOUT_S 20
// Use the SIM7000G's native HTTP(S) app (AT+SHREQ) for telemetry POSTs instead of raw
// TLS sockets, which drop ~half of HTTPS responses at this cadence (a documented modem
// limit). The native path is the primary; postJson() auto-falls-back to the raw-TLS
// path and self-disables native after repeated failures, so an unsupported/buggy modem
// firmware can never take the fleet dark. Set to 0 to force the old raw-TLS path.
#define TRACKER_USE_NATIVE_HTTPS 1
// 1NCE OS UDP transport. Telemetry goes as a single plain-UDP datagram to the 1NCE
// endpoint inside the carrier network (the SIM is the identity — no TLS on the device);
// 1NCE forwards it to /api/fleet/nce-webhook over HTTPS. Kills the per-post TLS
// handshake (~6-9KB each) that was burning the SIM data budget. A full HTTPS POST
// still runs every TRACKER_NCE_HTTPS_HEARTBEAT_MS for assured delivery + OTA offers,
// and every UDP failure falls back to HTTPS, so the tracker→server→map pipeline can
// never go dark from the UDP path alone. Off by default — enable per-board with a
// -D TRACKER_USE_NCE_UDP=1 build flag (bench trial: tracker-001 only).
#ifndef TRACKER_USE_NCE_UDP
#define TRACKER_USE_NCE_UDP 0
#endif
#define TRACKER_NCE_UDP_HOST "udp.os.1nce.com"
#define TRACKER_NCE_UDP_PORT 4445
#define TRACKER_NCE_UDP_MAX_PAYLOAD 508   // documented max safe UDP payload
#define TRACKER_NCE_HTTPS_HEARTBEAT_MS 900000UL  // 15 min

#define TRACKER_GNSS_WARMUP_MS 45000UL
#define TRACKER_GNSS_STATUS_LOG_MS 30000UL
#define TRACKER_GNSS_RECYCLE_MS 1800000UL
