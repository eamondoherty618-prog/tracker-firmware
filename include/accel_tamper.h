#pragma once

#include <ArduinoJson.h>

// ─── ADXL375 high-g accelerometer add-on (I2C 0x53) ──────────────────────────
// Optional hardware: probed once at boot; absent = module dormant, tracker
// unaffected. When present, a small FreeRTOS task samples continuously
// (100 Hz) and records impact/tamper events — the main loop never blocks on
// the sensor. INT1 is wired to GPIO 34 for future wake-from-deep-sleep
// support (stubbed, not used in v1).

// Probe the sensor and, when found, start the sampling task.
void accelInit();

// Contribute the optional "impact" section to a telemetry document.
// compact=true (UDP): only a recent-peak field, only when something happened.
// compact=false (HTTPS): presence flag, session peak, recent event list.
void accelAppendTelemetry(JsonDocument& doc, bool compact);

bool accelPresent();

// Future: arm INT1 (GPIO 34) as an ext0 wakeup source before deep sleep so a
// parked tracker can sleep hard and still catch a tow/impact. Deliberately a
// stub in v1 — deep sleep isn't part of the current power design.
void accelConfigureWakeOnInt1();
