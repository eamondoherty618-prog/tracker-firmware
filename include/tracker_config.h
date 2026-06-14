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
#define TRACKER_FIRMWARE_VERSION "0.9.18"

// Ignition detection via alternator voltage.
// Above ON threshold = engine running (alternator charging ~13.8V).
// Below OFF threshold = battery only. Hysteresis prevents bounce.
#define TRACKER_IGNITION_ON_MV  13200U
#define TRACKER_IGNITION_OFF_MV 12800U

// Tuning knobs.
#define TRACKER_MOVING_INTERVAL_MS 10000UL
#define TRACKER_PARKED_INTERVAL_MS 120000UL
#define TRACKER_MIN_MOVING_SPEED_KPH 6.0f
#define TRACKER_HARD_BRAKE_DELTA_KPH -22.0f
#define TRACKER_RAPID_ACCEL_DELTA_KPH 22.0f
#define TRACKER_EVENT_WINDOW_MS 12000UL
#define TRACKER_GNSS_WARMUP_MS 45000UL
#define TRACKER_GNSS_STATUS_LOG_MS 30000UL
#define TRACKER_GNSS_RECYCLE_MS 1800000UL
