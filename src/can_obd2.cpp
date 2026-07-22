#include "can_obd2.h"

#include <Arduino.h>
#include "driver/twai.h"
#include "tracker_config.h"

// ISO 15765-4 CAN for OBD2: 11-bit functional request 0x7DF answered from
// 0x7E8-0x7EF, with a 29-bit fallback (0x18DB33F1 → 0x18DAF1xx) for vehicles
// that use extended addressing. Single-frame for live PIDs; ISO-TP multi-frame
// (first frame → flow control → consecutive frames) for VIN and long DTC lists.

namespace {

enum class BusState : uint8_t {
  UNINSTALLED,   // driver not installed (boot, or torn down after failed probe)
  LISTENING,     // passive (LISTEN_ONLY): confirm healthy 500k traffic first
  PROBING,       // sending detection requests, waiting for any ECU to answer
  READY,         // bus confirmed — normal polling
  ABSENT,        // probe failed — dormant until the next re-probe window
};

enum class Op : uint8_t { NONE, PROBE, PID, DTC_STORED, DTC_PENDING, VIN, CLEAR };

BusState g_state = BusState::UNINSTALLED;
bool g_extended = false;          // 29-bit addressing in use
uint8_t g_probeAttempts = 0;
uint32_t g_nextProbeMs = 0;       // when ABSENT: earliest re-probe
uint32_t g_lastPollMs = 0;
uint32_t g_lastDtcPollMs = 0;

// In-flight request
Op g_op = Op::NONE;
uint8_t g_opPid = 0;
uint32_t g_opDeadlineMs = 0;
uint8_t g_pidIndex = 0;           // which configured PID the poll cycle is on

// Live values (NAN/negative = never read); *_ms = when last updated
const uint8_t kPids[] = TRACKER_OBD_PIDS;
constexpr size_t kPidCount = sizeof(kPids) / sizeof(kPids[0]);
float g_pidValue[kPidCount];
uint32_t g_pidMs[kPidCount];

// DTCs (decoded "P0301" style), VIN, clear handshake
constexpr size_t MAX_DTC = 8;
String g_dtc[MAX_DTC];      size_t g_dtcCount = 0;
String g_dtcPending[MAX_DTC]; size_t g_dtcPendingCount = 0;
bool g_dtcEverRead = false;
// New-code detection for the instant-post path: a stored-DTC set that changed
// vs the previous read this boot shouldn't wait out the telemetry cadence.
String g_dtcSig; bool g_dtcSigValid = false;
bool g_newDtcEvent = false;
String g_vin;
bool g_vinRequested = false;
bool g_clearRequested = false;
bool g_clearedFlag = false;       // reported once in the next full payload

// ISO-TP reassembly
struct IsoTp {
  bool active = false;
  uint32_t respId = 0;
  uint16_t expected = 0;
  uint16_t got = 0;
  uint8_t nextSn = 1;
  uint32_t lastMs = 0;
  uint8_t buf[64];
} g_isotp;

uint32_t functionalReqId() { return g_extended ? 0x18DB33F1UL : 0x7DFUL; }

bool isObdResponseId(uint32_t id, bool extd) {
  if (g_extended) return extd && (id & 0x1FFFFF00UL) == 0x18DAF100UL;
  return !extd && id >= 0x7E8 && id <= 0x7EF;
}

// Flow-control frame goes to the specific ECU that sent the first frame.
uint32_t flowControlId(uint32_t respId) {
  if (g_extended) {
    const uint8_t ecu = respId & 0xFF;             // 0x18DAF1<ecu>
    return 0x18DA0000UL | ((uint32_t)ecu << 8) | 0xF1UL;  // 0x18DA<ecu>F1
  }
  return respId - 8;                               // 0x7E8 → 0x7E0
}

bool sendFrame(uint32_t id, const uint8_t* data, uint8_t len) {
  twai_message_t msg = {};
  msg.identifier = id;
  msg.extd = g_extended ? 1 : 0;
  msg.data_length_code = 8;                        // ISO 15765-4: pad to 8
  memset(msg.data, 0x00, 8);
  memcpy(msg.data, data, len);
  const bool ok = twai_transmit(&msg, pdMS_TO_TICKS(20)) == ESP_OK;
#if TRACKER_BENCH_DEBUG
  Serial.printf("CAN TX %08lx:", (unsigned long)id);
  for (int i = 0; i < 8; i++) Serial.printf(" %02x", msg.data[i]);
  Serial.println(ok ? "" : "  (tx failed)");
#endif
  return ok;
}

bool sendRequest(const uint8_t* payload, uint8_t len) {
  uint8_t f[8] = { len };
  memcpy(f + 1, payload, len);
  return sendFrame(functionalReqId(), f, len + 1);
}

void beginOp(Op op, uint8_t pid, uint32_t timeoutMs) {
  g_op = op;
  g_opPid = pid;
  g_opDeadlineMs = millis() + timeoutMs;
  g_isotp.active = false;
}

bool startRequestForOp(Op op, uint8_t pid) {
  switch (op) {
    case Op::PROBE:
    case Op::PID: {
      const uint8_t r[] = { 0x01, pid };
      return sendRequest(r, 2);
    }
    case Op::DTC_STORED: { const uint8_t r[] = { 0x03 }; return sendRequest(r, 1); }
    case Op::DTC_PENDING: { const uint8_t r[] = { 0x07 }; return sendRequest(r, 1); }
    case Op::VIN: { const uint8_t r[] = { 0x09, 0x02 }; return sendRequest(r, 2); }
    case Op::CLEAR: { const uint8_t r[] = { 0x04 }; return sendRequest(r, 1); }
    default: return false;
  }
}

// Decode one DTC byte pair to the standard P/C/B/U code string.
String decodeDtc(uint8_t a, uint8_t b) {
  static const char kSys[4] = { 'P', 'C', 'B', 'U' };
  char code[6];
  snprintf(code, sizeof(code), "%c%01X%01X%01X%01X",
           kSys[(a >> 6) & 0x3], (a >> 4) & 0x3, a & 0xF, (b >> 4) & 0xF, b & 0xF);
  return String(code);
}

void parseDtcPayload(const uint8_t* d, uint16_t len, String* out, size_t* outCount) {
  // d[0] = 0x43/0x47, d[1] = count, then 2-byte pairs.
  *outCount = 0;
  if (len < 2) return;
  const uint8_t n = d[1];
  for (uint8_t i = 0; i < n && *outCount < MAX_DTC; i++) {
    const uint16_t off = 2 + i * 2;
    if (off + 1 >= len) break;
    if (d[off] == 0 && d[off + 1] == 0) continue;  // padding pair
    out[(*outCount)++] = decodeDtc(d[off], d[off + 1]);
  }
}

void handlePidValue(uint8_t pid, const uint8_t* ab, uint16_t len) {
  float v = NAN;
  switch (pid) {
    case 0x0C: if (len >= 2) v = ((ab[0] << 8) | ab[1]) / 4.0f; break;  // RPM
    case 0x0D: if (len >= 1) v = ab[0]; break;                          // km/h
    case 0x05: if (len >= 1) v = (float)ab[0] - 40.0f; break;           // coolant °C
    case 0x11: if (len >= 1) v = ab[0] * 100.0f / 255.0f; break;        // throttle %
    case 0x42: if (len >= 2) v = ((ab[0] << 8) | ab[1]) / 1000.0f; break; // control module voltage
    case 0x2F: if (len >= 1) v = ab[0] * 100.0f / 255.0f; break;        // fuel level %
    case 0x10: if (len >= 2) v = ((ab[0] << 8) | ab[1]) / 100.0f; break; // MAF grams/sec
    default:   if (len >= 1) v = ab[0]; break;   // raw first byte for extra PIDs
  }
  for (size_t i = 0; i < kPidCount; i++) {
    if (kPids[i] == pid && !isnan(v)) { g_pidValue[i] = v; g_pidMs[i] = millis(); }
  }
}

// A fully reassembled (or single-frame) OBD payload for the in-flight op.
void handleObdPayload(const uint8_t* d, uint16_t len) {
  if (len < 1) return;
  const uint8_t svc = d[0];
  switch (g_op) {
    case Op::PROBE:
      if (svc == 0x41) {
        g_state = BusState::READY;
        g_probeAttempts = 0;
        Serial.print("OBD2: bus detected (");
        Serial.print(g_extended ? "29" : "11");
        Serial.println("-bit)");
        if (len >= 4) handlePidValue(d[1], d + 2, len - 2);
      }
      break;
    case Op::PID:
      if (svc == 0x41 && len >= 3 && d[1] == g_opPid) handlePidValue(d[1], d + 2, len - 2);
      break;
    case Op::DTC_STORED:
      if (svc == 0x43) {
        parseDtcPayload(d, len, g_dtc, &g_dtcCount);
        g_dtcEverRead = true;
        String sig;
        for (size_t i = 0; i < g_dtcCount; i++) { sig += g_dtc[i]; sig += ','; }
        // First read of the boot only baselines; codes already present ride the
        // next normal post. A set that changes mid-session fires instantly.
        if (g_dtcSigValid && sig != g_dtcSig && g_dtcCount > 0) g_newDtcEvent = true;
        g_dtcSig = sig;
        g_dtcSigValid = true;
      }
      break;
    case Op::DTC_PENDING:
      if (svc == 0x47) parseDtcPayload(d, len, g_dtcPending, &g_dtcPendingCount);
      break;
    case Op::VIN:
      // [0x49, 0x02, 0x01, 17 VIN chars]
      if (svc == 0x49 && len >= 4) {
        String vin;
        for (uint16_t i = 3; i < len && vin.length() < 17; i++) {
          const char c = (char)d[i];
          if (c >= '0' && c <= 'Z') vin += c;
        }
        if (vin.length() == 17) { g_vin = vin; Serial.print("OBD2: VIN "); Serial.println(g_vin); }
      }
      break;
    case Op::CLEAR:
      if (svc == 0x44) {
        g_dtcCount = 0;
        g_dtcPendingCount = 0;
        g_dtcSig = "";
        g_clearedFlag = true;
        Serial.println("OBD2: stored DTCs cleared (server-commanded)");
      }
      break;
    default: break;
  }
  g_op = Op::NONE;
}

void handleFrame(const twai_message_t& msg) {
#if TRACKER_BENCH_DEBUG
  Serial.printf("CAN RX %08lx:", (unsigned long)msg.identifier);
  for (int i = 0; i < msg.data_length_code; i++) Serial.printf(" %02x", msg.data[i]);
  Serial.println();
#endif
  if (!isObdResponseId(msg.identifier, msg.extd)) return;
  const uint8_t* d = msg.data;
  const uint8_t pci = d[0] >> 4;

  if (pci == 0x0) {                                   // single frame
    const uint8_t len = d[0] & 0xF;
    if (len >= 1 && len <= 7) handleObdPayload(d + 1, len);
  } else if (pci == 0x1) {                            // first frame → flow control
    g_isotp.active = true;
    g_isotp.respId = msg.identifier;
    g_isotp.expected = ((d[0] & 0xF) << 8) | d[1];
    if (g_isotp.expected > sizeof(g_isotp.buf)) g_isotp.expected = sizeof(g_isotp.buf);
    g_isotp.got = 0;
    g_isotp.nextSn = 1;
    g_isotp.lastMs = millis();
    for (uint8_t i = 2; i < 8 && g_isotp.got < g_isotp.expected; i++) g_isotp.buf[g_isotp.got++] = d[i];
    const uint8_t fc[] = { 0x30, 0x00, 0x00 };        // continue, no block limit, no delay
    sendFrame(flowControlId(msg.identifier), fc, 3);
  } else if (pci == 0x2 && g_isotp.active && msg.identifier == g_isotp.respId) {  // consecutive
    if ((d[0] & 0xF) != (g_isotp.nextSn & 0xF)) { g_isotp.active = false; return; }
    g_isotp.nextSn++;
    g_isotp.lastMs = millis();
    for (uint8_t i = 1; i < 8 && g_isotp.got < g_isotp.expected; i++) g_isotp.buf[g_isotp.got++] = d[i];
    if (g_isotp.got >= g_isotp.expected) {
      g_isotp.active = false;
      handleObdPayload(g_isotp.buf, g_isotp.got);
    }
  }
}

bool installDriver(bool listenOnly) {
  // LISTEN_ONLY is physically passive: no TX, not even ACK bits. The probe
  // starts there so a mis-wired tap or wrong-bitrate bus can't be disturbed
  // by us — a live vehicle bus carries power steering and worse (field:
  // plugging an unpowered, terminated tap bumped the idle and latched a
  // "service power steering" warning).
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)TRACKER_CAN_TX_GPIO, (gpio_num_t)TRACKER_CAN_RX_GPIO,
      listenOnly ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL);
  g.rx_queue_len = 32;
  g.tx_queue_len = 4;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  if (twai_start() != ESP_OK) { twai_driver_uninstall(); return false; }
  return true;
}

void teardownDriver() {
  twai_stop();
  twai_driver_uninstall();
}

void goAbsent() {
  teardownDriver();
  g_state = BusState::ABSENT;
  g_nextProbeMs = millis() + TRACKER_OBD_REPROBE_MS;
  g_op = Op::NONE;
  Serial.println("OBD2: no bus detected — dormant (will re-probe when the vehicle is active)");
}

uint32_t g_listenStartMs = 0;
uint32_t g_listenFrames = 0;

void startProbe() {
  // Phase 1: passive listen. Healthy 500k traffic (frames received, no bus
  // errors) is required before we ever transmit a bit.
  if (g_state == BusState::UNINSTALLED || g_state == BusState::ABSENT) {
    if (!installDriver(true)) { g_state = BusState::ABSENT; g_nextProbeMs = millis() + TRACKER_OBD_REPROBE_MS; return; }
  }
  g_state = BusState::LISTENING;
  g_listenStartMs = millis();
  g_listenFrames = 0;
  g_probeAttempts = 0;
  g_extended = false;
  g_op = Op::NONE;
}

}  // namespace

void canObd2Init() {
  for (size_t i = 0; i < kPidCount; i++) { g_pidValue[i] = NAN; g_pidMs[i] = 0; }

#if TRACKER_BENCH_DEBUG
  // RX idle check: a powered, healthy transceiver holds RXD HIGH (recessive)
  // when the bus is quiet. LOW/floating here = dead unit logic or a broken
  // white lead — distinguishes them from ESP32-side faults without a meter.
  pinMode(TRACKER_CAN_RX_GPIO, INPUT);
  delay(5);
  Serial.printf("CAN: RX line idle = %s (HIGH = transceiver alive)\n",
                digitalRead(TRACKER_CAN_RX_GPIO) ? "HIGH" : "LOW");
#endif

#if TRACKER_BENCH_DEBUG && !TRACKER_CAN_LISTEN_ONLY_LOCK
  // Line-load test: drive TX as a plain GPIO and read it back. A healthy,
  // unpowered-or-listening transceiver input barely loads the pin; a damaged
  // TXD input clamps it, and the readback disagrees with what we drive.
  {
    pinMode(TRACKER_CAN_TX_GPIO, OUTPUT);
    digitalWrite(TRACKER_CAN_TX_GPIO, HIGH);
    delayMicroseconds(50);
    const int readHigh = digitalRead(TRACKER_CAN_TX_GPIO);
    digitalWrite(TRACKER_CAN_TX_GPIO, LOW);
    delayMicroseconds(50);
    const int readLow = digitalRead(TRACKER_CAN_TX_GPIO);
    digitalWrite(TRACKER_CAN_TX_GPIO, HIGH);  // leave recessive
    Serial.printf("CAN: TX line-load test: drive HIGH reads %s, drive LOW reads %s%s\n",
                  readHigh ? "HIGH" : "LOW", readLow ? "LOW(ok)" : "LOW(ok)",
                  readHigh ? " — line free" : " — LINE CLAMPED (unit TXD input suspect)");
    pinMode(TRACKER_CAN_TX_GPIO, INPUT);
  }

  // Desk self-test: transmit one frame in NO_ACK mode and listen for our own
  // echo through the transceiver. Straight TX/RX wiring = echo received;
  // crossed Grove leads = silence. This resolves the wiring question WITHOUT
  // a vehicle (bench only — this mode transmits, so it is compiled out of
  // bench_listen and must never run plugged into a live bus).
  {
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)TRACKER_CAN_TX_GPIO, (gpio_num_t)TRACKER_CAN_RX_GPIO, TWAI_MODE_NO_ACK);
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    bool pass = false;
    if (twai_driver_install(&g, &t, &f) == ESP_OK && twai_start() == ESP_OK) {
      twai_message_t tx = {};
      tx.identifier = 0x555;
      tx.self = 1;               // self-reception request: loop back via the bus pins
      tx.data_length_code = 2;
      tx.data[0] = 0xAB; tx.data[1] = 0xCD;
      if (twai_transmit(&tx, pdMS_TO_TICKS(100)) == ESP_OK) {
        twai_message_t rx;
        if (twai_receive(&rx, pdMS_TO_TICKS(300)) == ESP_OK && rx.identifier == 0x555) pass = true;
      }
      // Error autopsy: the counters say WHY an echo died. High bus/tx errors =
      // we transmitted but the bus never reflected it (transceiver mute or
      // dead driver). Clean counters + no echo = frame never made it out.
      twai_status_info_t st;
      if (twai_get_status_info(&st) == ESP_OK) {
        Serial.printf("CAN: post-test state=%d tx_err=%lu rx_err=%lu bus_err=%lu tx_failed=%lu arb_lost=%lu\n",
                      (int)st.state, (unsigned long)st.tx_error_counter, (unsigned long)st.rx_error_counter,
                      (unsigned long)st.bus_error_count, (unsigned long)st.tx_failed_count,
                      (unsigned long)st.arb_lost_count);
      }
      twai_stop();
      twai_driver_uninstall();
    }
    Serial.println(pass
      ? "CAN self-test: PASS — TX/RX wiring to the transceiver is good"
      : "CAN self-test: FAIL — no echo; yellow/white Grove leads likely crossed (or unit unpowered)");

    // Slow-speed retry: marginal/damaged analog paths sometimes pass at
    // 125 kbps while failing at 500. A split verdict is its own fingerprint.
    if (!pass) {
      twai_general_config_t g2 = TWAI_GENERAL_CONFIG_DEFAULT(
          (gpio_num_t)TRACKER_CAN_TX_GPIO, (gpio_num_t)TRACKER_CAN_RX_GPIO, TWAI_MODE_NO_ACK);
      twai_timing_config_t t2 = TWAI_TIMING_CONFIG_125KBITS();
      twai_filter_config_t f2 = TWAI_FILTER_CONFIG_ACCEPT_ALL();
      bool slowPass = false;
      if (twai_driver_install(&g2, &t2, &f2) == ESP_OK && twai_start() == ESP_OK) {
        twai_message_t tx2 = {};
        tx2.identifier = 0x556;
        tx2.self = 1;
        tx2.data_length_code = 1;
        tx2.data[0] = 0x5A;
        if (twai_transmit(&tx2, pdMS_TO_TICKS(100)) == ESP_OK) {
          twai_message_t rx2;
          if (twai_receive(&rx2, pdMS_TO_TICKS(300)) == ESP_OK && rx2.identifier == 0x556) slowPass = true;
        }
        twai_stop();
        twai_driver_uninstall();
      }
      Serial.println(slowPass
        ? "CAN self-test @125k: PASS — analog path marginal at 500k (damaged but limping)"
        : "CAN self-test @125k: FAIL — transmit path hard-dead at any speed");
    }
  }
#endif

  startProbe();
}

void canObd2RequestClearDtc() {
  if (g_state != BusState::READY) { Serial.println("OBD2: clear requested but bus not present"); return; }
  g_clearRequested = true;
}

bool canObd2Present() { return g_state == BusState::READY; }

bool canObd2TakeNewDtcEvent() {
  const bool e = g_newDtcEvent;
  g_newDtcEvent = false;
  return e;
}

bool canObd2EngineRunning() {
  // Fresh RPM (0x0C) reading above the idle floor = engine on.
  for (size_t i = 0; i < kPidCount; i++) {
    if (kPids[i] == 0x0C) {
      return g_pidMs[i] != 0 && (millis() - g_pidMs[i] < 15000UL) && g_pidValue[i] > 250.0f;
    }
  }
  return false;
}

void canObd2Service(bool vehicleActive) {
  const uint32_t now = millis();

  // Drain everything the controller has for us — cheap, no blocking.
  if (g_state == BusState::PROBING || g_state == BusState::READY) {
    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK) handleFrame(msg);

    // A wedged controller (no transceiver → no ACKs → bus-off) reads as absent.
    twai_status_info_t st;
    if (twai_get_status_info(&st) == ESP_OK && st.state == TWAI_STATE_BUS_OFF) {
      goAbsent();
      return;
    }
  }

  // ISO-TP reassembly timeout
  if (g_isotp.active && now - g_isotp.lastMs > 500) g_isotp.active = false;

  switch (g_state) {
    case BusState::ABSENT:
      // Re-probe on a timer regardless of trip state (the listen phase is
      // passive and harmless on a sleeping bus). Field lesson: an unlucky
      // boot window marked the bus absent and, gated on GPS-motion trips,
      // never retried while the truck idled in the driveway for an hour.
      if ((int32_t)(now - g_nextProbeMs) >= 0) startProbe();
      return;
    case BusState::UNINSTALLED:
      return;
    case BusState::LISTENING: {
#if TRACKER_BENCH_DEBUG
      // Silence must be visible: prove the listener is alive even when the
      // bus is dead quiet (parked truck, desk bench, unplugged tap).
      static uint32_t lastListenLogMs = 0;
      if (now - lastListenLogMs >= 10000UL) {
        lastListenLogMs = now;
        Serial.printf("CAN: listening (%s), %lu frames heard\n",
                      TRACKER_CAN_LISTEN_ONLY_LOCK ? "locked passive" : "pre-probe",
                      (unsigned long)g_listenFrames);
      }
#endif
      // Count anything heard; healthy traffic ⇒ safe to speak. Silence for
      // the whole window ⇒ treat as absent (or parked) and stay passive.
      twai_message_t msg;
      while (twai_receive(&msg, 0) == ESP_OK) {
        g_listenFrames++;
#if TRACKER_BENCH_DEBUG
        if (g_listenFrames <= 5) {
          Serial.printf("CAN LISTEN %08lx dlc=%d\n", (unsigned long)msg.identifier, msg.data_length_code);
        }
#endif
      }
      if (TRACKER_CAN_LISTEN_ONLY_LOCK) {
        // Locked passive: report what we hear, never speak. Frame count goes
        // out in telemetry so the first vehicle session is verifiable without
        // a laptop.
        return;
      }
      // Threshold/window tuned on a live 2017 Silverado: the main loop blocks
      // for seconds at a time during boot (modem bring-up), so early drains
      // only catch a queue's worth of frames — 5s was expiring before 20
      // frames ever accumulated even on a roaring bus. 5 real frames is
      // already unambiguous; give the window 30s to survive boot congestion.
      if (g_listenFrames >= 5) {
        // Real traffic at 500k confirmed — switch to normal mode and probe.
        teardownDriver();
        if (!installDriver(false)) { g_state = BusState::ABSENT; g_nextProbeMs = now + TRACKER_OBD_REPROBE_MS; return; }
        Serial.println("OBD2: bus traffic heard — probing");
        g_state = BusState::PROBING;
        g_probeAttempts = 0;
        g_extended = false;
      } else if (now - g_listenStartMs > 30000) {
        goAbsent();
      }
      return;
    }
    case BusState::PROBING: {
      if (g_op == Op::PROBE && (int32_t)(now - g_opDeadlineMs) < 0) return;  // await answer
      if (g_op == Op::PROBE) {                       // timed out
        g_op = Op::NONE;
        g_probeAttempts++;
        if (g_probeAttempts >= 3) {
          if (!g_extended) { g_extended = true; g_probeAttempts = 0; }  // try 29-bit
          else { goAbsent(); return; }
        }
      }
      beginOp(Op::PROBE, kPids[0], 400);
      startRequestForOp(Op::PROBE, kPids[0]);
      return;
    }
    case BusState::READY:
      break;
  }

  // READY: one in-flight request at a time, next step chosen by priority.
  if (g_op != Op::NONE) {
    if ((int32_t)(now - g_opDeadlineMs) < 0) return;   // still waiting
    g_op = Op::NONE;                                    // timed out — move on
  }

  if (g_clearRequested) {
    g_clearRequested = false;
    beginOp(Op::CLEAR, 0, 800);
    startRequestForOp(Op::CLEAR, 0);
    return;
  }
  if (!g_vinRequested) {
    g_vinRequested = true;
    beginOp(Op::VIN, 0, 800);
    startRequestForOp(Op::VIN, 0);
    return;
  }
  if (vehicleActive && now - g_lastDtcPollMs >= TRACKER_OBD_DTC_POLL_MS) {
    // Alternate stored/pending each window to keep bus chatter minimal.
    static bool pendingTurn = false;
    g_lastDtcPollMs = now;
    const Op op = pendingTurn ? Op::DTC_PENDING : Op::DTC_STORED;
    pendingTurn = !pendingTurn;
    beginOp(op, 0, 800);
    startRequestForOp(op, 0);
    return;
  }
  // PIDs poll on every READY bus, not just on trips: parked-with-engine-on is
  // exactly the idling state the fleet features bill by, and trip detection is
  // GPS-motion-based so it can never see it. Parked polls run at a relaxed
  // cadence to keep bus chatter and data cost down.
  const uint32_t pollGap = vehicleActive ? (TRACKER_OBD_POLL_MS / kPidCount)
                                         : TRACKER_OBD_PARKED_POLL_MS;
  if (now - g_lastPollMs >= pollGap) {
    g_lastPollMs = now;
    const uint8_t pid = kPids[g_pidIndex];
    g_pidIndex = (g_pidIndex + 1) % kPidCount;
    beginOp(Op::PID, pid, 300);
    startRequestForOp(Op::PID, pid);
  }
}

void canObd2AppendTelemetry(JsonDocument& doc, bool compact) {
  const uint32_t now = millis();
  const bool present = canObd2Present();

  if (compact) {
    if (!present) return;                              // UDP budget: omit entirely
    JsonObject obd = doc["obd"].to<JsonObject>();
    for (size_t i = 0; i < kPidCount; i++) {
      if (g_pidMs[i] == 0 || now - g_pidMs[i] > 30000UL) continue;  // stale
      switch (kPids[i]) {
        case 0x0C: obd["rpm"] = (int)g_pidValue[i]; break;
        case 0x0D: obd["speed_kph"] = (int)g_pidValue[i]; break;
        case 0x05: obd["coolant_c"] = (int)g_pidValue[i]; break;
        case 0x11: obd["throttle_pct"] = (int)g_pidValue[i]; break;
        // Voltage + fuel ride the compact/UDP payload too: they drive the
        // battery-health, low-fuel, and fuel-theft alerts, which must fire
        // without waiting for a full HTTPS heartbeat.
        case 0x42: obd["volt"] = serialized(String(g_pidValue[i], 1)); break;
        case 0x2F: obd["fuel_pct"] = (int)g_pidValue[i]; break;
        default: break;
      }
    }
    if (g_dtcEverRead) obd["dtc_count"] = (int)g_dtcCount;
    return;
  }

  JsonObject obd = doc["obd"].to<JsonObject>();
  obd["present"] = present;
  if (g_state == BusState::LISTENING && g_listenFrames > 0) {
    obd["listen_frames"] = g_listenFrames;  // passive session: bus heard, not spoken to
  }
  if (!present) return;
  obd["addr"] = g_extended ? "29bit" : "11bit";
  for (size_t i = 0; i < kPidCount; i++) {
    if (g_pidMs[i] == 0 || now - g_pidMs[i] > 30000UL) continue;
    switch (kPids[i]) {
      case 0x0C: obd["rpm"] = (int)g_pidValue[i]; break;
      case 0x0D: obd["speed_kph"] = (int)g_pidValue[i]; break;
      case 0x05: obd["coolant_c"] = (int)g_pidValue[i]; break;
      case 0x11: obd["throttle_pct"] = (int)g_pidValue[i]; break;
      case 0x42: obd["volt"] = serialized(String(g_pidValue[i], 2)); break;      // control module V
      case 0x2F: obd["fuel_pct"] = (int)g_pidValue[i]; break;                    // fuel level %
      case 0x10: obd["maf_gps"] = serialized(String(g_pidValue[i], 1)); break;   // for MPG calc
      default: { char k[8]; snprintf(k, sizeof(k), "pid_%02x", kPids[i]); obd[k] = g_pidValue[i]; }
    }
  }
  if (g_vin.length() == 17) obd["vin"] = g_vin;
  if (g_dtcEverRead) {
    JsonArray dtc = obd["dtc"].to<JsonArray>();
    for (size_t i = 0; i < g_dtcCount; i++) dtc.add(g_dtc[i]);
    JsonArray pend = obd["dtc_pending"].to<JsonArray>();
    for (size_t i = 0; i < g_dtcPendingCount; i++) pend.add(g_dtcPending[i]);
  }
  if (g_clearedFlag) { obd["dtc_cleared"] = true; g_clearedFlag = false; }
}
