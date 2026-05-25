#pragma once

// Cellular SIM settings. Many IoT SIMs use an empty user/password.
#define TRACKER_APN_PRIMARY "iot.1nce.net"
#define TRACKER_APN_FALLBACK "iot.1nce.net"
#define TRACKER_GPRS_USER ""
#define TRACKER_GPRS_PASS ""

// Temporary Cloudflare quick tunnel fronting the Windows ops box.
#define TRACKER_SERVER_HOST "theme-trailers-usd-ryan.trycloudflare.com"
#define TRACKER_SERVER_HOST_HEADER "theme-trailers-usd-ryan.trycloudflare.com"
#define TRACKER_SERVER_PORT 443
#define TRACKER_SERVER_PATH "/api/fleet/telemetry"

// Use a real per-device token in production.
#define TRACKER_DEVICE_ID "tracker-002"
#define TRACKER_FIRMWARE_VERSION "0.1.0"
#define TRACKER_AUTH_TOKEN "dev-token-change-me"

// Tuning knobs.
#define TRACKER_MOVING_INTERVAL_MS 10000UL
#define TRACKER_PARKED_INTERVAL_MS 60000UL
#define TRACKER_MIN_MOVING_SPEED_KPH 6.0f
#define TRACKER_HARD_BRAKE_DELTA_KPH -22.0f
#define TRACKER_RAPID_ACCEL_DELTA_KPH 22.0f
#define TRACKER_EVENT_WINDOW_MS 12000UL
#define TRACKER_GNSS_WARMUP_MS 45000UL
#define TRACKER_GNSS_STATUS_LOG_MS 30000UL
#define TRACKER_GNSS_RECYCLE_MS 300000UL
