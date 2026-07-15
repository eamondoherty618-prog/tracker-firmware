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
    case 0x05: if (len >= 1) v = (float)ab[0] - 40.0f; break;           // °C
    case 0x11: if (len >= 1) v = ab[0] * 100.0f / 255.0f; break;        // %
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
      if (svc == 0x43) { parseDtcPayload(d, len, g_dtc, &g_dtcCount); g_dtcEverRead = true; }
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

bool installDriver() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)TRACKER_CAN_TX_GPIO, (gpio_num_t)TRACKER_CAN_RX_GPIO, TWAI_MODE_NORMAL);
  g.rx_queue_len = 16;
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

void startProbe() {
  if (g_state == BusState::UNINSTALLED || g_state == BusState::ABSENT) {
    if (!installDriver()) { g_state = BusState::ABSENT; g_nextProbeMs = millis() + TRACKER_OBD_REPROBE_MS; return; }
  }
  g_state = BusState::PROBING;
  g_probeAttempts = 0;
  g_extended = false;
  g_op = Op::NONE;
}

}  // namespace

void canObd2Init() {
  for (size_t i = 0; i < kPidCount; i++) { g_pidValue[i] = NAN; g_pidMs[i] = 0; }
  startProbe();
}

void canObd2RequestClearDtc() {
  if (g_state != BusState::READY) { Serial.println("OBD2: clear requested but bus not present"); return; }
  g_clearRequested = true;
}

bool canObd2Present() { return g_state == BusState::READY; }

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
      // Re-probe only while the vehicle is active: a parked car's bus is
      // silent, and "silent" must not be confused with "not wired".
      if (vehicleActive && (int32_t)(now - g_nextProbeMs) >= 0) startProbe();
      return;
    case BusState::UNINSTALLED:
      return;
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
  if (vehicleActive && now - g_lastPollMs >= TRACKER_OBD_POLL_MS / kPidCount) {
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
        default: break;
      }
    }
    if (g_dtcEverRead) obd["dtc_count"] = (int)g_dtcCount;
    return;
  }

  JsonObject obd = doc["obd"].to<JsonObject>();
  obd["present"] = present;
  if (!present) return;
  obd["addr"] = g_extended ? "29bit" : "11bit";
  for (size_t i = 0; i < kPidCount; i++) {
    if (g_pidMs[i] == 0 || now - g_pidMs[i] > 30000UL) continue;
    switch (kPids[i]) {
      case 0x0C: obd["rpm"] = (int)g_pidValue[i]; break;
      case 0x0D: obd["speed_kph"] = (int)g_pidValue[i]; break;
      case 0x05: obd["coolant_c"] = (int)g_pidValue[i]; break;
      case 0x11: obd["throttle_pct"] = (int)g_pidValue[i]; break;
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
