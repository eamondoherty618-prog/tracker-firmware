#pragma once

#include <ArduinoJson.h>

// ─── CAN bus / OBD2 add-on (M5Stack Mini CAN, TJA1051T on TWAI) ──────────────
// Optional hardware: the tracker boots and runs normally without it. Presence
// is detected at runtime (an OBD2 request must be answered); absent buses are
// re-probed when the vehicle is active, so a tracker installed before the CAN
// tap is wired starts reporting as soon as it can hear the bus.
//
// All calls are non-blocking. canObd2Service() is a cooperative state machine
// driven from the main loop — it never waits on the bus, so GPS/LTE cadence is
// unaffected.

// Install the driver and start the first presence probe. Safe to call when the
// transceiver isn't wired — probing fails cleanly and the module goes dormant.
void canObd2Init();

// Drive the state machine: probing, PID polling, DTC/VIN reads, remote-
// commanded DTC clear. `vehicleActive` gates polling (and re-probing) so a
// parked car's silent bus isn't treated as "hardware absent".
void canObd2Service(bool vehicleActive);

// Contribute the optional "obd" section to a telemetry document.
// compact=true (UDP datagram, 508-byte budget): live values only.
// compact=false (HTTPS): adds presence flag, VIN, stored/pending DTCs.
void canObd2AppendTelemetry(JsonDocument& doc, bool compact);

// Server-commanded Mode 04 (clear stored DTCs + MIL). The clear ONLY happens
// through this path — there is deliberately no local/automatic trigger.
void canObd2RequestClearDtc();

bool canObd2Present();

// True when a fresh OBD reading shows the engine turning (rpm above idle
// floor). This is a real engine-on signal — unlike GPS motion — so the main
// loop uses it to keep posting fast while idling, not just while moving.
bool canObd2EngineRunning();

// Consume-once: true when the stored-DTC set changed since the previous read
// this boot. Drives the instant-post path so a fresh check-engine code reaches
// the server in seconds instead of at the next cadence tick.
bool canObd2TakeNewDtcEvent();

// Heartbeat size guard: skip the bus-survey section in the NEXT full payload
// build (the modem's POST body cap is ~1 KB; oversized bodies can never send).
void canObd2SuppressSurveyOnce();

// Consume-once: crank-sag minimum control-module voltage (PID 0x42) captured
// over the 5s window after the last engine start, or NAN if none pending.
// Trended server-side to flag a battery about to die.
float canObd2TakeCrankVMin();
