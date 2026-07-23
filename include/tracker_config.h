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
// The default is deliberately "unprovisioned": a board flashed without explicit
// -D flags names itself tracker-<last 6 hex of its efuse MAC> on first boot
// (factory flow) instead of silently impersonating a fleet unit. The fleet
// tracker-001/002/003 envs pass their IDs via build flags.
#ifndef TRACKER_DEVICE_ID
#define TRACKER_DEVICE_ID "unprovisioned"
#endif
#ifndef TRACKER_API_KEY
#define TRACKER_API_KEY "c5cc56a23546eb487223fe810ae8a8b76d83376ee09140f7"
#endif
#define TRACKER_FIRMWARE_VERSION "0.11.8"

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
// never go dark from the UDP path alone. Fleet default ON as of v0.10.1 — proven
// end-to-end on the bench (tracker-001, 2026-07-05). NOTE: a SIM whose data session
// predates the org's 1NCE OS activation isn't routed to the UDP endpoint until it
// re-attaches (fix: Reset connection in the portal / POST /v1/sims/{iccid}/reset);
// the HTTPS fallback covers tracking either way. Set 0 to force HTTPS-only.
#ifndef TRACKER_USE_NCE_UDP
#define TRACKER_USE_NCE_UDP 1
#endif
#define TRACKER_NCE_UDP_HOST "udp.os.1nce.com"
// DNS fallbacks — the hostname resolves to 1NCE-internal IPs that some PDP
// sessions' modem DNS fails to resolve (field: truck fell back to HTTPS every
// post). 1NCE says these can change, so hostname is tried first.
#define TRACKER_NCE_UDP_IP1 "10.60.2.239"
#define TRACKER_NCE_UDP_IP2 "10.60.8.90"
#define TRACKER_NCE_UDP_PORT 4445
#define TRACKER_NCE_UDP_MAX_PAYLOAD 508   // documented max safe UDP payload
#define TRACKER_NCE_HTTPS_HEARTBEAT_MS 900000UL  // 15 min
// When the server reports our datagrams aren't arriving (1NCE session not
// UDP-routed — seen twice in the field), post via HTTPS for this long before
// retrying UDP.
#define TRACKER_NCE_UDP_COOLDOWN_MS 3600000UL  // 60 min

#define TRACKER_GNSS_WARMUP_MS 45000UL
#define TRACKER_GNSS_STATUS_LOG_MS 30000UL
#define TRACKER_GNSS_RECYCLE_MS 1800000UL

// ── Optional add-on hardware (runtime-detected; absent = module dormant) ─────
// CAN/OBD2 via M5Stack Mini CAN (TJA1051T) on the ESP32 TWAI controller.
// Pins 32/33 are free on the T-SIM7000G (modem UART 26/27, SD 2/13/14/15).
#define TRACKER_CAN_TX_GPIO 32
#define TRACKER_CAN_RX_GPIO 33
// Mode 01 PIDs polled round-robin while the vehicle is active. Decoders exist
// for 0x0C rpm / 0x0D speed / 0x05 coolant / 0x11 throttle; other PIDs report
// their raw first byte as pid_XX.
#define TRACKER_OBD_PIDS { 0x0C, 0x0D, 0x05, 0x11, 0x42, 0x2F, 0x10 }  // rpm,speed,coolant,throttle,voltage,fuel,MAF
#define TRACKER_OBD_POLL_MS 5000UL       // full PID rotation period (on a trip)
#define TRACKER_OBD_PARKED_POLL_MS 30000UL // per-PID gap while parked (idle detection)
#define TRACKER_OBD_DTC_POLL_MS 60000UL  // stored/pending DTCs alternate here
#define TRACKER_OBD_REPROBE_MS 300000UL  // absent bus: retry while driving
// Post cadence while the engine is running (rpm-confirmed), idling or moving,
// so the live vitals dash stays fresh. Highway still uses the 5s branch.
#define TRACKER_OBD_ACTIVE_INTERVAL_MS 10000UL

// ADXL375 high-g accelerometer (impact/tamper). INT1 wired to GPIO 34 for
// future wake-from-deep-sleep (v1 leaves it unused).
#define TRACKER_ACCEL_SDA 21
#define TRACKER_ACCEL_SCL 22
#define TRACKER_ACCEL_ADDR 0x53
#define TRACKER_ACCEL_INT1_GPIO 34
#define TRACKER_IMPACT_THRESHOLD_G 6.0f  // dynamic g (gravity removed) that counts as an impact
#define TRACKER_IMPACT_DEBOUNCE_MS 2000UL
// At/above this peak the impact is treated as a crash candidate: the main loop
// posts immediately over reliable HTTPS and the server escalates (push + SMS).
// 6-12 g = pothole/door-slam territory; a real collision on a chassis mount
// reads well above this. Severity grading happens server-side from the peak.
#define TRACKER_CRASH_THRESHOLD_G 12.0f

// Bench verification: 1 = dump raw CAN frames + live accel readings to serial.
// Build with the bench_debug pio env rather than editing this.
#ifndef TRACKER_BENCH_DEBUG
#define TRACKER_BENCH_DEBUG 0
#endif
// First-live-vehicle guard: 1 = CAN stays in LISTEN_ONLY permanently — hears
// and reports traffic but can never transmit (not even ACK bits). Use the
// bench_listen env for the first session on a real vehicle; switch to the
// normal build only after clean traffic is confirmed.
#ifndef TRACKER_CAN_LISTEN_ONLY_LOCK
#define TRACKER_CAN_LISTEN_ONLY_LOCK 0
#endif
