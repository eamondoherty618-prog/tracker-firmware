#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <MD5Builder.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGsmClient.h>
#include <Update.h>
#include <WiFi.h>

#include "tracker_config.h"

// Fleet tracker GATT service — advertised continuously so the mobile app can
// discover and claim this tracker via Bluetooth without typing a device ID.
#define FLEET_BLE_SERVICE_UUID  "1230FACE-0001-4A7E-9C00-000000000001"
#define FLEET_BLE_DEVINFO_UUID  "1230FACE-0002-4A7E-9C00-000000000001"

namespace {

// LilyGo T-SIM7000G v1.x defaults.
constexpr int MODEM_TX = 27;
constexpr int MODEM_RX = 26;
constexpr int MODEM_PWRKEY = 4;
constexpr int MODEM_BAUD = 115200;

// Onboard microSD slot (SPI). Used only at boot to look for a firmware.bin to
// self-flash — the field-recovery path when a unit can't be reached over USB.
constexpr int SD_SCLK = 14;
constexpr int SD_MISO = 2;
constexpr int SD_MOSI = 15;
constexpr int SD_CS   = 13;

constexpr size_t QUEUE_DEPTH = 24;

HardwareSerial SerialAT(1);
TinyGsm modem(SerialAT);
TinyGsmClientSecure secureClient(modem, 0);

// Runtime credentials — populated from NVS if provisioned, otherwise fall back
// to the compile-time TRACKER_DEVICE_ID / TRACKER_API_KEY values.
String g_deviceId;
String g_apiKey;
bool g_provisioned = false;
// Adoption latch (NVS-persisted, server-driven): false until the server's
// telemetry response reports this device is assigned to a vehicle in some org.
// Unadopted boards advertise over BLE continuously so the app can claim them;
// adopted boards only advertise while on a trip (driver presence). Un-claiming
// the device in the app flips this back and the board becomes adoptable again.
bool g_adopted = false;

struct QueuedMessage {
  String body;
  uint32_t queuedAtMs = 0;
};

QueuedMessage queue[QUEUE_DEPTH];
size_t queueHead = 0;
size_t queueCount = 0;

uint32_t lastTelemetryMs = 0;
uint32_t lastPostSuccessMs = 0;  // watchdog: reset on every confirmed POST
constexpr uint32_t POST_WATCHDOG_MS = 90UL * 1000UL;   // 90s — recover fast from dead data sessions
uint8_t telemetryCycleCount = 0;
// Diagnostics surfaced in telemetry so we can see what happens mid-drive without a serial cable.
uint8_t g_consecutivePostFails = 0;   // resets on success
uint16_t g_watchdogCount = 0;         // times the POST watchdog power-cycled the modem
uint16_t g_forcedReconnects = 0;      // times GPRS was torn down after repeated post fails
uint32_t g_bootCount = 0;             // persisted across reboots (NVS) — detects brownout/crash resets
uint32_t lastSpeedSampleMs = 0;
float lastSpeedKph = NAN;
bool gpsEnabled = false;
bool gprsConnected = false;
uint32_t lastNetworkWaitMs = 0;
uint32_t gpsEnabledAtMs = 0;
uint32_t lastGnssStatusLogMs = 0;
bool lastGpsFix = false;
bool lastGpsQualityFix = false;
float lastKnownLat = NAN;
float lastKnownLon = NAN;
char lastKnownTimestamp[25] = "";
float lastKnownCourse = NAN;  // degrees from North, computed from consecutive fixes
float prevFixLat = NAN;       // lat of previous quality fix (for heading computation)
float prevFixLon = NAN;
uint32_t lastFixMs = 0;       // millis() when last quality fix was stored
// Stationary position lock — freeze reported position when parked to eliminate GPS drift.
uint32_t stationaryStartMs = 0;
float stationaryLat = NAN;
float stationaryLon = NAN;
constexpr uint32_t STATIONARY_LOCK_MS = 30000UL;  // lock after 30s stopped
constexpr float STATIONARY_SPEED_KPH = 2.0f;      // below this = stationary
uint32_t lastAgpsMs = 0;
uint32_t gpsFirstFixMs = 0;
bool ignitionOn = false;  // true when on a trip (GPS motion within the grace window)
uint32_t ignitionOffSinceMs = 0;  // millis() when ignition last went off (0 = on/unknown)
uint32_t lastMovingMs = 0;        // millis() of the last GPS reading at/above moving speed
// Smart-sleep: once the engine's been off this long, drop to a slow heartbeat. The
// heartbeat stays well under the app's 35-min offline cutoff, so the vehicle keeps
// showing "parked" (not "offline") while it sits, at ~5x less parked power/data.
constexpr uint32_t DEEP_PARK_AFTER_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t PARKED_HEARTBEAT_MS = 10UL * 60UL * 1000UL;
uint16_t g_lastBattMv = 0;
BLECharacteristic* g_bleDevInfoChar = nullptr;

// Hardware loop watchdog — reboots the board if the main loop stops making progress
// (e.g. a blocking call like WiFi.scanNetworks() or a modem read hangs). The software
// recovery can't help a HUNG loop because it only runs when postJson() returns; this
// timer ISR fires independently and forces a reboot. (Field freeze: stopped posting
// mid-drive with watchdog_count=0, forced_reconnects=0, no reboot = loop was hung.)
hw_timer_t* g_loopWdt = nullptr;
volatile uint32_t g_loopBeat = 0;

// Hang breadcrumb — records the operation currently running in RTC memory (survives a
// software/watchdog reset). On the next boot we read what it was doing when it hung,
// plus the reset reason, and report both in telemetry to pinpoint the freeze source.
RTC_NOINIT_ATTR char g_breadcrumb[24];
RTC_NOINIT_ATTR uint32_t g_breadcrumbMagic;
char g_lastHangOp[24] = "";     // breadcrumb from before the last reset (reported once)
char g_lastBuiltMotion[12] = ""; // motion_state of the most recently built telemetry payload
// 1NCE UDP health: sends since the last HTTPS heartbeat, consecutive local
// send failures, the server's report of when a datagram last ARRIVED
// (udp_age_s; -2 = no report yet), and the HTTPS-only cooldown deadline.
uint32_t g_udpSentSinceHb = 0;
uint32_t g_udpFailStreak = 0;
long g_lastServerUdpAgeS = -2;
uint32_t g_udpDisabledUntilMs = 0;
const char* g_resetReason = ""; // why the board last reset (sw/panic/brownout/poweron)
#define CRUMB(s) do { strncpy(g_breadcrumb, (s), sizeof(g_breadcrumb) - 1); g_breadcrumb[sizeof(g_breadcrumb) - 1] = 0; } while (0)

void powerOnModem() {
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(100);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1100);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(3000);
}

// Forward declaration — defined later, but needed here to disable modem sleep
// the moment the modem first responds (both at boot and after a watchdog reset).
bool sendSimpleAt(const String& command, uint32_t timeoutMs);

bool waitForModem() {
  Serial.print("Waiting for modem");
  for (int attempt = 0; attempt < 20; attempt++) {
    if (modem.testAT(1000)) {
      Serial.println(" ok");
      // Disable all modem power-saving modes — the SIM7000G and 1NCE network
      // can negotiate PSM / UART sleep, making the modem unreachable mid-drive.
      sendSimpleAt("+CSCLK=0", 3000);   // disable UART sleep
      sendSimpleAt("+CPSMS=0", 3000);   // disable PSM (network power saving)
      return true;
    }
    Serial.print(".");
    delay(500);
  }
  Serial.println(" failed");
  return false;
}

void logAtCommand(const char* command, uint32_t timeoutMs = 3000) {
  while (SerialAT.available()) {
    SerialAT.read();
  }

  Serial.print("AT");
  Serial.println(command);
  SerialAT.print("AT");
  SerialAT.print(command);
  SerialAT.print("\r\n");

  const uint32_t deadline = millis() + timeoutMs;
  String line;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        line.trim();
        if (line.length() > 0) {
          Serial.print("  ");
          Serial.println(line);
          if (line == "OK" || line == "ERROR") {
            return;
          }
        }
        line = "";
      } else {
        line += c;
      }
    }
    delay(5);
  }

  line.trim();
  if (line.length() > 0) {
    Serial.print("  ");
    Serial.println(line);
  }
  Serial.println("  <timeout>");
}

void drainSerialAt() {
  while (SerialAT.available()) {
    SerialAT.read();
  }
}

bool waitForLineContaining(const char* token, String& matchedLine, uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  String line;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        line.trim();
        if (line.length() > 0) {
          if (line.indexOf(token) >= 0) {
            matchedLine = line;
            return true;
          }
          line = "";
        }
      } else {
        line += c;
      }
    }
    delay(5);
  }
  matchedLine = "";
  return false;
}

bool waitForOk(uint32_t timeoutMs) {
  String matchedLine;
  return waitForLineContaining("OK", matchedLine, timeoutMs);
}

bool waitForPromptChar(char target, uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == target) {
        Serial.print("Prompt: ");
        Serial.println(target);
        return true;
      }
      if (c != '\r' && c != '\n') {
        Serial.print("Prompt saw: ");
        Serial.println(c);
      }
    }
    delay(5);
  }
  Serial.println("Prompt: <timeout>");
  return false;
}

bool waitForOkVerbose(uint32_t timeoutMs, const char* context) {
  const uint32_t deadline = millis() + timeoutMs;
  String line;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        line.trim();
        if (line.length() > 0) {
          Serial.print(context);
          Serial.print(": ");
          Serial.println(line);
          if (line == "OK") {
            return true;
          }
          if (line == "ERROR") {
            return false;
          }
          line = "";
        }
      } else {
        line += c;
      }
    }
    delay(5);
  }

  line.trim();
  if (line.length() > 0) {
    Serial.print(context);
    Serial.print(": ");
    Serial.println(line);
    if (line == "OK") {
      return true;
    }
  }

  Serial.print(context);
  Serial.println(": <timeout>");
  return false;
}

bool waitForHttpAction(String& actionLine, uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  String line;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        line.trim();
        if (line.length() > 0) {
          Serial.print("HTTPACTION response: ");
          Serial.println(line);
          if (line.startsWith("+HTTPACTION:")) {
            actionLine = line;
            return true;
          }
          if (line == "ERROR") {
            return false;
          }
          line = "";
        }
      } else {
        line += c;
      }
    }
    delay(5);
  }

  line.trim();
  if (line.startsWith("+HTTPACTION:")) {
    Serial.print("HTTPACTION response: ");
    Serial.println(line);
    actionLine = line;
    return true;
  }

  Serial.println("HTTPACTION response: <timeout>");
  actionLine = "";
  return false;
}

bool sendSimpleAt(const String& command, uint32_t timeoutMs = 5000) {
  drainSerialAt();
  SerialAT.print("AT");
  SerialAT.print(command);
  SerialAT.print("\r\n");
  return waitForOk(timeoutMs);
}

bool sendVerboseAt(const String& command, uint32_t timeoutMs = 5000) {
  Serial.print(">>> AT"); Serial.println(command);
  drainSerialAt();
  SerialAT.print("AT");
  SerialAT.print(command);
  SerialAT.print("\r\n");
  return waitForOkVerbose(timeoutMs, "    ");
}

// AT+CASEND's completion signal varies by modem firmware revision: some return
// an async "+CASEND:" URC, this SIM7000G (R1529) just returns plain "OK" and
// signals incoming data separately via "+CADATAIND:". Accept either as success
// so the upload doesn't hang waiting for a URC this firmware never sends.
bool waitForCasendComplete(uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  String line;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == '\r') continue;
      if (c == '\n') {
        line.trim();
        if (line.length() > 0) {
          if (line == "OK" || line.indexOf("+CASEND:") >= 0) return true;
          if (line == "ERROR") return false;
          line = "";
        }
      } else {
        line += c;
      }
    }
    delay(5);
  }
  return false;
}

// AT+CARECV's response is "+CARECV: <len>,<raw bytes...>" — the payload that
// follows the prefix is NOT line-oriented text; it's the literal socket data
// (HTTP headers + binary body for an OTA download), which routinely contains
// '\n' bytes of its own. Reading it with a line-based parser silently
// truncates/corrupts chunks at the first embedded newline. Parse the "+CARECV:
// <len>," prefix as a raw byte sequence, then read exactly <len> bytes after
// it — no line semantics involved. Returns bytes copied into buf, or -1 on
// timeout/protocol mismatch (no trailing comma).
int readCarecvChunk(uint8_t* buf, int bufCap, uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  const char* needle = "+CARECV: ";
  const int needleLen = 9;
  int matched = 0;
  while (millis() < deadline && matched < needleLen) {
    if (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == needle[matched]) matched++;
      else matched = (c == needle[0]) ? 1 : 0;
    } else {
      delay(1);
    }
  }
  if (matched != needleLen) return -1;

  int len = 0;
  bool sawDigit = false;
  bool sawComma = false;
  while (millis() < deadline && !sawComma) {
    if (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c >= '0' && c <= '9') { len = len * 10 + (c - '0'); sawDigit = true; }
      else if (c == ',') sawComma = true;
    } else {
      delay(1);
    }
  }
  if (!sawDigit || !sawComma) return -1;
  if (len > bufCap) len = bufCap;

  int got = 0;
  while (got < len && millis() < deadline) {
    if (SerialAT.available()) buf[got++] = (uint8_t)SerialAT.read();
    else delay(1);
  }
  waitForOk(2000);
  return got;
}

static uint8_t nmeaXorChecksum(const char* sentence) {
  uint8_t crc = 0;
  for (int i = 1; sentence[i] && sentence[i] != '*'; i++)
    crc ^= static_cast<uint8_t>(sentence[i]);
  return crc;
}

bool sendGnssNmea(const char* sentence) {
  char full[80];
  snprintf(full, sizeof(full), "%s*%02X", sentence, nmeaXorChecksum(sentence));
  Serial.print("GNSS cmd: "); Serial.println(full);
  drainSerialAt();
  SerialAT.print("AT+CGNSCMD=0,\"");
  SerialAT.print(full);
  SerialAT.print("\"\r\n");
  return waitForOk(2000);
}

// Configures the GNSS engine after power-on: enables GLONASS alongside GPS,
// SBAS/WAAS differential corrections, and bumps the update rate to 2 Hz.
void configureGnss() {
  Serial.println("GNSS: configuring GPS+GLONASS + SBAS + 2 Hz...");
  sendGnssNmea("$PMTK353,1,1,0,0,0");  // GPS + GLONASS only (matches CGNSMOD; all-4 hurts TTFF on SIM7000G)
  delay(100);
  sendGnssNmea("$PMTK313,1");           // Enable SBAS search
  sendGnssNmea("$PMTK301,2");           // SBAS integrity mode (WAAS / EGNOS)
  delay(100);
  sendGnssNmea("$PMTK220,500");         // 2 Hz position update rate
  sendGnssNmea("$PMTK513,1");           // Enable autonomous EPO prediction
  drainSerialAt();
}

bool sendAtAndWaitForToken(const String& command, const char* token, uint32_t timeoutMs) {
  drainSerialAt();
  SerialAT.print("AT");
  SerialAT.print(command);
  SerialAT.print("\r\n");
  String matchedLine;
  if (waitForLineContaining(token, matchedLine, timeoutMs)) {
    Serial.print("AT");
    Serial.print(command);
    Serial.print(" -> ");
    Serial.println(matchedLine);
    return true;
  }
  Serial.print("AT");
  Serial.print(command);
  Serial.print(" -> <timeout waiting for ");
  Serial.print(token);
  Serial.println(">");
  return false;
}

bool waitForTokenVerbose(const char* token, uint32_t timeoutMs, const char* context) {
  String matchedLine;
  if (waitForLineContaining(token, matchedLine, timeoutMs)) {
    Serial.print(context);
    Serial.print(": ");
    Serial.println(matchedLine);
    return true;
  }
  Serial.print(context);
  Serial.print(": <timeout waiting for ");
  Serial.print(token);
  Serial.println(">");
  return false;
}

void logSimDiagnostics() {
  Serial.println("SIM/radio diagnostics:");
  logAtCommand("+CMEE=2");
  logAtCommand("+CPIN?");
  logAtCommand("+CSMINS?");
  logAtCommand("+CCID");
  logAtCommand("+CIMI");
  logAtCommand("+CSQ");
  logAtCommand("+CEREG?");
  logAtCommand("+CREG?");
  logAtCommand("+COPS?");
  logAtCommand("+CBAND?");
  logAtCommand("+CPSI?");
}

void configureNetworkMode() {
  Serial.print("SIM status: ");
  Serial.println(static_cast<int>(modem.getSimStatus()));

  Serial.print("Supported network modes: ");
  Serial.println(modem.getNetworkModes());
  Serial.print("Supported preferred modes: ");
  Serial.println(modem.getPreferredModes());

  Serial.println("Configuring modem for automatic network + Cat-M/NB-IoT...");
  Serial.println(modem.setNetworkMode(2) ? "CNMP automatic ok" : "CNMP automatic failed");
  Serial.println(modem.setPreferredMode(3) ? "CMNB CAT-M/NB-IoT ok" : "CMNB CAT-M/NB-IoT failed");
  modem.setNetworkSystemMode(true);
  delay(1000);

  Serial.print("Network mode now: ");
  Serial.println(modem.getNetworkMode());
  Serial.print("Preferred mode now: ");
  Serial.println(modem.getPreferredMode());
}

bool ensureNetwork() {
  if (modem.isNetworkConnected()) {
    return true;
  }

  const uint32_t nowMs = millis();
  if (lastNetworkWaitMs != 0 && nowMs - lastNetworkWaitMs < 30000UL) {
    return false;
  }
  lastNetworkWaitMs = nowMs;

  Serial.print("Waiting for cellular network. SIM=");
  Serial.print(static_cast<int>(modem.getSimStatus()));
  Serial.print(" RSSI=");
  Serial.print(modem.getSignalQuality());
  Serial.print(" reg=");
  Serial.println(static_cast<int>(modem.getRegistrationStatus()));

  CRUMB("waitNet");
  // 25s (was 120s) — a shorter block means a coverage blip is a brief gap, and the
  // loop cycles back to retry/recover faster instead of sitting dark for two minutes.
  const bool connected = modem.waitForNetwork(25000L, true);
  Serial.print("Network wait result: ");
  Serial.println(connected ? "connected" : "not registered");
  if (connected) {
    Serial.print("Operator: ");
    Serial.println(modem.getOperator());
    bool autoReport = false;
    int16_t systemMode = 0;
    if (modem.getNetworkSystemMode(autoReport, systemMode)) {
      Serial.print("System mode: ");
      Serial.println(systemMode);
    }
  }
  return connected;
}

void connectGprs() {
  if (modem.isGprsConnected()) {
    gprsConnected = true;
    return;
  }

  if (!ensureNetwork()) {
    gprsConnected = false;
    return;
  }

  Serial.print("Connecting cellular data with APN ");
  Serial.println(TRACKER_APN_PRIMARY);
  gprsConnected = modem.gprsConnect(TRACKER_APN_PRIMARY, TRACKER_GPRS_USER, TRACKER_GPRS_PASS);
  if (!gprsConnected && String(TRACKER_APN_FALLBACK) != String(TRACKER_APN_PRIMARY)) {
    Serial.print("Primary APN failed, trying ");
    Serial.println(TRACKER_APN_FALLBACK);
    modem.gprsDisconnect();
    delay(1000);
    gprsConnected = modem.gprsConnect(TRACKER_APN_FALLBACK, TRACKER_GPRS_USER, TRACKER_GPRS_PASS);
  }
  Serial.println(gprsConnected ? "GPRS connected" : "GPRS failed");
}

struct CellInfo {
  bool valid = false;
  char radioType[8] = "";  // "lte" | "gsm" | "wcdma"
  int mcc = 0;
  int mnc = 0;
  int lac = 0;   // TAC for LTE, LAC for GSM (field name is the same in MLS API)
  long cid = 0;  // Serving Cell ID
};

// Reads AT+CPSI? and parses fields: mode, MCC-MNC (f2), TAC/LAC (f3), Cell ID (f4).
CellInfo getCellInfo() {
  CRUMB("cellInfo");
  CellInfo info;
  drainSerialAt();
  SerialAT.print("AT+CPSI?\r\n");
  const uint32_t deadline = millis() + 5000;
  String line;
  bool done = false;
  while (!done && millis() < deadline) {
    while (SerialAT.available()) {
      const char c = SerialAT.read();
      if (c == '\r') continue;
      if (c == '\n') {
        line.trim();
        if (line.startsWith("+CPSI:")) {
          String s = line.substring(6);
          s.trim();
          // Fields (comma-separated): mode, opMode, MCC-MNC, TAC/LAC, CID, ...
          int c0 = s.indexOf(',');
          int c1 = (c0 >= 0) ? s.indexOf(',', c0 + 1) : -1;
          int c2 = (c1 >= 0) ? s.indexOf(',', c1 + 1) : -1;
          int c3 = (c2 >= 0) ? s.indexOf(',', c2 + 1) : -1;
          int c4 = (c3 >= 0) ? s.indexOf(',', c3 + 1) : -1;
          if (c3 < 0) { done = true; break; }
          String f0 = s.substring(0, c0);
          String f2 = s.substring(c1 + 1, c2);
          String f3 = s.substring(c2 + 1, c3);
          String f4 = (c4 >= 0) ? s.substring(c3 + 1, c4) : s.substring(c3 + 1);
          f0.trim(); f2.trim(); f3.trim(); f4.trim();
          String m = f0; m.toLowerCase();
          if (m.indexOf("no service") >= 0 || m.indexOf("search") >= 0) { done = true; break; }
          if (m.indexOf("lte") >= 0 || m.indexOf("cat") >= 0 || m.indexOf("nb") >= 0) {
            strncpy(info.radioType, "lte", sizeof(info.radioType));
          } else if (m.indexOf("gsm") >= 0 || m.indexOf("edge") >= 0) {
            strncpy(info.radioType, "gsm", sizeof(info.radioType));
          } else if (m.indexOf("wcdma") >= 0) {
            strncpy(info.radioType, "wcdma", sizeof(info.radioType));
          } else { done = true; break; }
          int dash = f2.indexOf('-');
          if (dash > 0) { info.mcc = f2.substring(0, dash).toInt(); info.mnc = f2.substring(dash + 1).toInt(); }
          if (f3.startsWith("0x") || f3.startsWith("0X")) {
            info.lac = (int)strtol(f3.c_str() + 2, nullptr, 16);
          } else {
            info.lac = f3.toInt();
          }
          info.cid = f4.toInt();
          info.valid = (info.mcc > 0);
        }
        if (line == "OK" || line == "ERROR") done = true;
        line = "";
      } else { line += c; }
    }
    if (!done) delay(5);
  }
  return info;
}

// Downloads satellite orbit predictions from SIMCom's AGPS server over the active
// GPRS connection. Reduces cold-start TTFF from ~10 min to under 30 seconds.
bool downloadAgps() {
  if (!gprsConnected) return false;
  CRUMB("agps");
  Serial.println("AGPS: fetching assistance data...");
  drainSerialAt();
  SerialAT.print("AT+CAGPS\r\n");

  const uint32_t deadline = millis() + 60000;
  String line;
  bool gotData = false;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      const char c = static_cast<char>(SerialAT.read());
      if (c == '\r') continue;
      if (c == '\n') {
        line.trim();
        if (line.length() > 0) {
          Serial.print("AGPS: "); Serial.println(line);
          if (line.startsWith("+CAGPS:")) gotData = true;
          if (line == "OK") { lastAgpsMs = millis(); return gotData; }
          if (line == "ERROR" || line.indexOf("+CME ERROR") >= 0) {
            Serial.println("AGPS: not supported or server error");
            // Back off on failure so we don't hammer AT+CAGPS every loop and
            // starve the GNSS receiver of modem time. Retry on the normal 24h cycle.
            lastAgpsMs = millis();
            return false;
          }
          line = "";
        }
      } else { line += c; }
    }
    delay(5);
  }
  Serial.println("AGPS: timeout");
  lastAgpsMs = millis();  // back off after a timeout too
  return false;
}

// Persists the last quality GPS fix to NVS so it can be re-injected into the GNSS
// engine on the next boot — gives the chip a position hypothesis immediately, cutting
// the satellite-search cone from hemisphere-wide to a few hundred km.
void saveGnssPosition(float lat, float lon, float alt,
                      int year, int month, int day,
                      int hour, int min, int sec) {
  Preferences prefs;
  prefs.begin("gnss", false);
  prefs.putFloat("lat",   lat);
  prefs.putFloat("lon",   lon);
  prefs.putFloat("alt",   alt);
  prefs.putInt("year",  year);
  prefs.putInt("month", month);
  prefs.putInt("day",   day);
  prefs.putInt("hour",  hour);
  prefs.putInt("min",   min);
  prefs.putInt("sec",   sec);
  prefs.end();
}

void injectSavedPosition() {
  Preferences prefs;
  prefs.begin("gnss", true);
  const float lat  = prefs.getFloat("lat",  NAN);
  const float lon  = prefs.getFloat("lon",  NAN);
  const float alt  = prefs.getFloat("alt",  0.0f);
  const int year   = prefs.getInt("year",   0);
  const int month  = prefs.getInt("month",  0);
  const int day    = prefs.getInt("day",    0);
  const int hour   = prefs.getInt("hour",   0);
  const int min    = prefs.getInt("min",    0);
  const int sec    = prefs.getInt("sec",    0);
  prefs.end();

  if (isnan(lat) || isnan(lon) || year == 0) {
    Serial.println("GNSS NVS: no saved position");
    return;
  }
  Serial.print("GNSS NVS: injecting "); Serial.print(lat, 5);
  Serial.print(","); Serial.println(lon, 5);
  // PMTK740: lat, lon, alt, uncertainty_m, YYYYMMDD, HHMMSS.000, speed_ms
  // 1000 m uncertainty tells the engine this is a rough hint, not a precise fix.
  char sentence[128];
  snprintf(sentence, sizeof(sentence),
    "$PMTK740,%.6f,%.6f,%.1f,1000.0,%04d%02d%02d,%02d%02d%02d.000,0",
    lat, lon, alt, year, month, day, hour, min, sec);
  sendGnssNmea(sentence);
}

// Routine GNSS enables must NEVER wipe aiding data. A $PMTK104 (full cold start)
// erases ephemeris/almanac/EPO/last-position and forces a hemisphere-wide search —
// the root cause of perpetual no-lock. We gate the factory reset behind a one-shot
// NVS flag, used only as explicit recovery from a suspected-corrupt GNSS NVRAM
// config. Set bool "factory_reset"=true in the "gnss" namespace to request one;
// it clears itself after running a single time.
bool consumeGnssFactoryResetRequest() {
  Preferences prefs;
  prefs.begin("gnss", false);
  const bool requested = prefs.getBool("factory_reset", false);
  if (requested) prefs.putBool("factory_reset", false);  // one-shot
  prefs.end();
  return requested;
}

void ensureGps() {
  if (gpsEnabled) {
    return;
  }

  Serial.println("Enabling GNSS...");
  gpsEnabled = modem.enableGPS();   // AT+CGNSPWR=1 — powers receiver + active antenna
  Serial.println(gpsEnabled ? "GNSS enabled" : "GNSS enable failed");
  if (gpsEnabled) {
    gpsEnabledAtMs = millis();
    lastGnssStatusLogMs = 0;
    gpsFirstFixMs = 0;

    // Power receiver + active antenna and pick a reliable constellation set first.
    // GPS+GLONASS is what the SIM7000G locks most reliably; enabling all four
    // (adding BeiDou+Galileo) degrades acquisition on this chip.
    sendVerboseAt("+CGNSPWR=1", 3000);          // power the GNSS receiver
    sendVerboseAt("+CGNSMOD=1,1,0,0", 3000);    // GPS + GLONASS
    sendVerboseAt("+CGPIO=0,48,1,1", 2000);     // drive modem GPIO4 HIGH = power the ACTIVE GPS antenna LDO (T-SIM7000G)
    delay(200);

    // Explicit, gated recovery ONLY — never wipe aiding data on a routine enable.
    if (consumeGnssFactoryResetRequest()) {
      Serial.println("GNSS: one-time factory reset requested — clearing NVRAM config");
      sendGnssNmea("$PMTK104");   // full cold start; wipes ephemeris/EPO/position
      delay(1500);
    }

    configureGnss();              // constellation/SBAS/EPO NMEA cfg + update rate
    injectSavedPosition();        // warm-start position+time hint from NVS (PMTK740)
    // EPO (AT+CAGPS) + injected position/time are now in place, so the engine
    // performs a fast aided/warm start rather than a cold hemisphere-wide search.
  }
}

void logGnssStatus() {
  if (!gpsEnabled) {
    return;
  }

  const String raw = modem.getGPSraw();
  Serial.print("GNSS raw: ");
  if (raw.length() == 0) {
    Serial.println("<empty>");
  } else {
    Serial.println(raw);
  }
}

void restartGps() {
  Serial.println("Restarting GNSS engine...");
  modem.disableGPS();
  delay(1000);
  gpsEnabled = false;
  gpsFirstFixMs = 0;
  ensureGps();
}

void maintainGps() {
  ensureGps();
  if (!gpsEnabled) {
    return;
  }

  const uint32_t nowMs = millis();
  if (lastGnssStatusLogMs == 0 || nowMs - lastGnssStatusLogMs >= TRACKER_GNSS_STATUS_LOG_MS) {
    lastGnssStatusLogMs = nowMs;
    const String raw = modem.getGPSraw();
    Serial.print("GNSS raw: ");
    Serial.println(raw.length() ? raw : "<empty>");
    // enableGPS() can report success while the engine never actually starts (CGNSINF
    // run-status field stays 0) — especially after a modem power-cycle — leaving GNSS
    // stuck off forever since ensureGps() early-returns on gpsEnabled. Detect the
    // engine-off state and re-kick it (CGNSPWR off->on) instead of waiting 30 min.
    static uint8_t gnssOffStreak = 0;
    if (raw.length() > 0 && raw[0] == '0') {
      if (++gnssOffStreak >= 2) {
        Serial.println("GNSS engine reported OFF while enabled — re-kicking");
        gnssOffStreak = 0;
        restartGps();
        return;
      }
    } else {
      gnssOffStreak = 0;
    }
  }

  // Refresh AGPS every 24h regardless of fix status — keeps satellite predictions fresh.
  if (gprsConnected && (lastAgpsMs == 0 || nowMs - lastAgpsMs >= 86400000UL)) {
    downloadAgps();
  }

  // Only recycle GNSS if no fix at all — a marginal fix means the engine is working.
  if (!lastGpsFix && gpsEnabledAtMs != 0 &&
      nowMs - gpsEnabledAtMs >= TRACKER_GNSS_RECYCLE_MS) {
    restartGps();
  }
}

String motionState(float speedKph) {
  if (isnan(speedKph)) {
    return "unknown";
  }
  // Engine running = on a trip, even when momentarily stopped (red light, stop-and-go).
  // Only call it "parked" when the engine is off AND it's below the moving threshold.
  if (ignitionOn) return "moving";
  return speedKph >= TRACKER_MIN_MOVING_SPEED_KPH ? "moving" : "parked";
}

String trackerDeviceId() {
  return g_provisioned ? g_deviceId : String(TRACKER_DEVICE_ID);
}

String trackerApiKey() {
  return g_provisioned ? g_apiKey : String(TRACKER_API_KEY);
}

String trackerHardwareId() {
  const uint64_t chipId = ESP.getEfuseMac();
  char value[17];
  snprintf(value, sizeof(value), "%04X%08X",
           static_cast<uint16_t>(chipId >> 32),
           static_cast<uint32_t>(chipId));
  return String(value);
}

String classifyEvent(float speedKph, uint32_t nowMs) {
  if (isnan(speedKph) || isnan(lastSpeedKph) || lastSpeedSampleMs == 0) {
    lastSpeedKph = speedKph;
    lastSpeedSampleMs = nowMs;
    return "";
  }

  const uint32_t dt = nowMs - lastSpeedSampleMs;
  const float delta = speedKph - lastSpeedKph;
  lastSpeedKph = speedKph;
  lastSpeedSampleMs = nowMs;

  // The two samples must be a sane gap apart: too short = GPS-speed jitter, too long
  // = they aren't a single braking/accel event. Outside that band, don't classify.
  if (dt < TRACKER_EVENT_MIN_DT_MS || dt > TRACKER_EVENT_WINDOW_MS) {
    return "";
  }

  // Rate, normalised by the ACTUAL gap (kph per second): negative = decel.
  const float rateKphPerS = delta / (dt / 1000.0f);

  // Require BOTH a steep enough rate AND a meaningful absolute change, so neither a
  // gentle long coast (low rate) nor a one-sample GPS wiggle (tiny delta) fires.
  if (rateKphPerS <= TRACKER_HARD_BRAKE_RATE_KPH_S && delta <= -TRACKER_EVENT_MIN_DELTA_KPH) {
    return "hard_brake";
  }
  if (rateKphPerS >= TRACKER_RAPID_ACCEL_RATE_KPH_S && delta >= TRACKER_EVENT_MIN_DELTA_KPH) {
    return "rapid_accel";
  }
  return "";
}

void enqueue(const String& body) {
  if (queueCount == QUEUE_DEPTH) {
    queue[queueHead].body = body;
    queue[queueHead].queuedAtMs = millis();
    queueHead = (queueHead + 1) % QUEUE_DEPTH;
    return;
  }

  const size_t index = (queueHead + queueCount) % QUEUE_DEPTH;
  queue[index].body = body;
  queue[index].queuedAtMs = millis();
  queueCount++;
}

bool dequeue(String& body) {
  if (queueCount == 0) {
    return false;
  }

  body = queue[queueHead].body;
  queue[queueHead] = {};
  queueHead = (queueHead + 1) % QUEUE_DEPTH;
  queueCount--;
  return true;
}

bool semverGt(const String& a, const String& b) {
  int av[3] = {0, 0, 0}, bv[3] = {0, 0, 0};
  String ta = a, tb = b;
  for (int i = 0; i < 3; i++) {
    int d = ta.indexOf('.');
    av[i] = (d < 0 ? ta : ta.substring(0, d)).toInt();
    if (d < 0) break;
    ta = ta.substring(d + 1);
  }
  for (int i = 0; i < 3; i++) {
    int d = tb.indexOf('.');
    bv[i] = (d < 0 ? tb : tb.substring(0, d)).toInt();
    if (d < 0) break;
    tb = tb.substring(d + 1);
  }
  for (int i = 0; i < 3; i++) {
    if (av[i] > bv[i]) return true;
    if (av[i] < bv[i]) return false;
  }
  return false;
}

// Scans raw HTTP response bytes for header/body boundary and Content-Length.
// Note: does NOT reset contentLength if no Content-Length header is found —
// this server doesn't send one (HTTP/2 origin has no need for it), so the
// caller's sizeHint (from the OTA offer's JSON) is the only source of truth
// and must survive untouched.
bool parseHttpHeadersForOta(const uint8_t* buf, int len, int& contentLength, int& bodyStart) {
  bodyStart = -1;
  for (int i = 0; i <= len - 4; i++) {
    if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
      bodyStart = i + 4;
      break;
    }
  }
  if (bodyStart < 0) return false;
  const char* cl = "content-length:";
  for (int i = 0; i < bodyStart - 15; i++) {
    bool match = true;
    for (int j = 0; j < 15; j++) {
      if (tolower((unsigned char)buf[i + j]) != cl[j]) { match = false; break; }
    }
    if (match) {
      int s = i + 15;
      while (s < bodyStart && buf[s] == ' ') s++;
      contentLength = 0;
      while (s < bodyStart && buf[s] >= '0' && buf[s] <= '9')
        contentLength = contentLength * 10 + (buf[s++] - '0');
      break;
    }
  }
  return true;
}

// The OTA server has no Content-Length to give (its origin is HTTP/2, which
// doesn't need one), so the HTTP/1.1 response the modem actually receives
// over the raw socket uses chunked transfer-encoding instead: each chunk is
// "<size-in-hex>\r\n<size bytes of data>\r\n", terminated by a "0\r\n\r\n"
// chunk. Without this, the literal "<hex>\r\n" framing bytes get written
// straight into the OTA partition, corrupting the image immediately.
struct ChunkedOtaDecoder {
  enum State { READ_SIZE, READ_DATA, READ_DATA_CRLF, DONE } state = READ_SIZE;
  size_t remaining = 0;
  String sizeHex;
  bool ok = true;

  // Feeds raw bytes (as received from CARECV) through the decoder, writing
  // decoded firmware bytes straight to Update.write(). Returns false only on
  // an actual Update.write() failure (sets ok=false so the caller can bail).
  void feed(const uint8_t* buf, int len) {
    int i = 0;
    while (i < len && state != DONE && ok) {
      switch (state) {
        case READ_SIZE: {
          const char c = static_cast<char>(buf[i++]);
          if (c == '\r') break;
          if (c == '\n') {
            remaining = strtoul(sizeHex.c_str(), nullptr, 16);
            sizeHex = "";
            state = (remaining == 0) ? DONE : READ_DATA;
          } else {
            sizeHex += c;
          }
          break;
        }
        case READ_DATA: {
          const int take = (int)min((size_t)(len - i), remaining);
          if (take > 0) {
            if (Update.write(const_cast<uint8_t*>(buf + i), take) != (size_t)take) { ok = false; break; }
            i += take;
            remaining -= take;
          }
          if (remaining == 0) state = READ_DATA_CRLF;
          break;
        }
        case READ_DATA_CRLF: {
          const char c = static_cast<char>(buf[i++]);
          if (c == '\n') state = READ_SIZE;
          break;
        }
        default: break;
      }
    }
  }
};

void downloadAndApplyOta(const String& version, int sizeHint, const String& md5 = "") {
  Serial.print("OTA: downloading v"); Serial.println(version);

  sendSimpleAt("+CACLOSE=0", 2000);
  if (!sendSimpleAt("+CACID=0", 5000)) return;
  sendSimpleAt("+CSSLCFG=\"sslversion\",0,3", 5000);
  sendSimpleAt("+CSSLCFG=\"ignorertctime\",0,1", 5000);
  sendSimpleAt("+CASSLCFG=0,ssl,1", 5000);
  sendAtAndWaitForToken("+CSSLCFG=\"ctxindex\",0", "+CSSLCFG:", 5000);
  waitForOkVerbose(5000, "ota-ssl");
  sendSimpleAt("+CASSLCFG=0,protocol,0", 5000);
  sendSimpleAt(String("+CSSLCFG=\"sni\",0,\"") + TRACKER_SERVER_HOST + "\"", 5000);

  String matchedLine;
  drainSerialAt();
  SerialAT.print("AT+CAOPEN=0,\""); SerialAT.print(TRACKER_SERVER_HOST);
  SerialAT.print("\","); SerialAT.print(TRACKER_SERVER_PORT); SerialAT.print("\r\n");
  if (!waitForLineContaining("+CAOPEN:", matchedLine, 75000)) {
    Serial.println("OTA: CAOPEN timeout"); return;
  }
  const int oc = matchedLine.lastIndexOf(',');
  if (oc < 0 || matchedLine.substring(oc + 1).toInt() != 0) {
    Serial.println("OTA: CAOPEN failed"); sendSimpleAt("+CACLOSE=0", 3000); return;
  }

  const String req = String("GET /api/fleet/ota/firmware?version=") + version +
    " HTTP/1.1\r\nHost: " + TRACKER_SERVER_HOST +
    "\r\nUser-Agent: " + trackerDeviceId() + "/" + TRACKER_FIRMWARE_VERSION +
    "\r\nx-tracker-key: " + trackerApiKey() +
    "\r\n\r\n";

  drainSerialAt();
  SerialAT.print("AT+CASEND=0,"); SerialAT.print(req.length()); SerialAT.print("\r\n");
  if (!waitForPromptChar('>', 10000)) { sendSimpleAt("+CACLOSE=0", 3000); return; }
  SerialAT.write(reinterpret_cast<const uint8_t*>(req.c_str()), req.length());
  SerialAT.flush();
  if (!waitForCasendComplete(30000)) {
    Serial.println("OTA: CASEND no response");
    sendSimpleAt("+CACLOSE=0", 3000); return;
  }
  Serial.println("OTA: request sent, awaiting headers...");
  delay(3000);

  static uint8_t otaBuf[1024];
  int contentLength = sizeHint > 0 ? sizeHint : -1;
  int bodyStart = -1;
  int firstChunkRead = 0;

  // The GET response isn't always ready the instant CASEND completes — retry
  // CARECV a few times (matches the body-read loop's pattern below) instead
  // of giving up on the first empty chunk.
  int headerRetries = 0;
  while (bodyStart < 0 && headerRetries < 10) {
    g_loopBeat++;  // OTA download legitimately runs long — keep the loop watchdog fed
    drainSerialAt();
    SerialAT.print("AT+CARECV=0,1024\r\n");
    const int n = readCarecvChunk(otaBuf, sizeof(otaBuf), 8000);
    if (n <= 0) {
      headerRetries++;
      delay(500);
      continue;
    }
    firstChunkRead = n;
    parseHttpHeadersForOta(otaBuf, firstChunkRead, contentLength, bodyStart);
    Serial.print("OTA: Content-Length="); Serial.print(contentLength);
    Serial.print(" bodyStart="); Serial.println(bodyStart);
  }

  if (bodyStart < 0 || contentLength <= 0) {
    Serial.println("OTA: bad headers, aborting");
    sendSimpleAt("+CACLOSE=0", 3000); return;
  }

  if (!Update.begin((size_t)contentLength)) {
    Serial.print("OTA: Update.begin failed: "); Serial.println(Update.errorString());
    sendSimpleAt("+CACLOSE=0", 3000); return;
  }
  if (md5.length() == 32) {
    Update.setMD5(md5.c_str());
    Serial.print("OTA: MD5 check: "); Serial.println(md5);
  }

  ChunkedOtaDecoder decoder;
  const int firstBodyLen = firstChunkRead - bodyStart;
  if (firstBodyLen > 0) {
    decoder.feed(otaBuf + bodyStart, firstBodyLen);
  }

  int retries = 0;
  while (decoder.ok && Update.progress() < (size_t)contentLength && retries < 300) {
    g_loopBeat++;  // OTA download legitimately runs long — keep the loop watchdog fed
    drainSerialAt();
    SerialAT.print("AT+CARECV=0,1024\r\n");
    const int bytesRead = readCarecvChunk(otaBuf, sizeof(otaBuf), 10000);
    if (bytesRead <= 0) { delay(500); retries++; continue; }
    retries = 0;

    decoder.feed(otaBuf, bytesRead);
    Serial.print("OTA: "); Serial.print(Update.progress()); Serial.print("/"); Serial.println(contentLength);
  }

  sendSimpleAt("+CACLOSE=0", 5000);

  if (!decoder.ok) {
    Serial.print("OTA: write error: "); Serial.println(Update.errorString());
    Update.abort(); return;
  }
  if (Update.progress() < (size_t)contentLength) {
    Serial.print("OTA: incomplete — got "); Serial.print(Update.progress());
    Serial.print("/"); Serial.println(contentLength);
    Update.abort(); return;
  }

  if (!Update.end(true)) {
    Serial.print("OTA: Update.end failed: "); Serial.println(Update.errorString());
    return;
  }

  Serial.println("OTA: SUCCESS — rebooting in 3s...");
  delay(3000);
  ESP.restart();
}

// Scans nearby WiFi networks without connecting. Returns a JSON array of
// {ssid, bssid, rssi} objects (up to 10 strongest), or "[]" on failure.
// Puts the radio to sleep again when done.
String scanWifi() {
  CRUMB("wifiScan");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // ASYNC scan + bounded poll. A blocking WiFi.scanNetworks() can wedge the entire
  // main loop indefinitely (the mid-drive freeze). Start it async and abandon after
  // 8s so it can NEVER hang. Feed the loop watchdog while we wait.
  WiFi.scanNetworks(true /*async*/, false /*hidden*/);
  const uint32_t deadline = millis() + 8000;
  int n = WIFI_SCAN_RUNNING;
  while (millis() < deadline) {
    n = WiFi.scanComplete();
    if (n != WIFI_SCAN_RUNNING) break;  // done (>=0) or failed (-2)
    g_loopBeat++;
    delay(100);
  }
  if (n < 0) {  // still running (timed out) or failed
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi scan: timed out/failed — skipping");
    return "[]";
  }
  if (n == 0) {
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi scan: no networks found");
    return "[]";
  }

  Serial.print("WiFi scan: "); Serial.print(n); Serial.println(" networks");
  JsonDocument scanDoc;
  JsonArray arr = scanDoc.to<JsonArray>();
  const int count = (n < 20) ? n : 20;
  for (int i = 0; i < count; i++) {
    JsonObject net = arr.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["bssid"] = WiFi.BSSIDstr(i);
    net["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);

  String result;
  serializeJson(scanDoc, result);
  return result;
}

// Configure SSL and open a persistent TLS connection to the server.
bool ensureClientConnected() {
  if (secureClient.connected()) return true;
  secureClient.stop();
  delay(500);

  // SSL configuration — must be set before each connect attempt.
  sendSimpleAt("+CACID=0", 5000);
  sendSimpleAt("+CSSLCFG=\"sslversion\",0,3", 3000);
  sendSimpleAt("+CSSLCFG=\"ignorertctime\",0,1", 3000);
  sendSimpleAt("+CSSLCFG=\"sni\",0,\"" + String(TRACKER_SERVER_HOST) + "\"", 3000);
  sendSimpleAt("+CASSLCFG=0,ssl,1", 5000);
  sendSimpleAt("+CASSLCFG=0,protocol,0", 5000);

  Serial.println("TLS: connecting...");
  CRUMB("tlsConnect");
  if (!secureClient.connect(TRACKER_SERVER_HOST, TRACKER_SERVER_PORT,
                            TRACKER_TLS_CONNECT_TIMEOUT_S)) {
    Serial.println("TLS: connect failed");
    return false;
  }
  Serial.println("TLS: connected");
  return true;
}

// Shared: harvest an OTA offer, adoption state, and/or a WiFi-geolocation fix
// from the server's reply. Used by both the native (SHREQ) and raw-TLS POST paths.
void updateAdoptionFromServer(bool adoptedNow);  // defined with the BLE code below

void handleTelemetryResponse(const String& respBodyIn) {
  const int jsonIdx = respBodyIn.indexOf('{');
  if (jsonIdx < 0) return;
  const String respBody = respBodyIn.substring(jsonIdx);

  JsonDocument respDoc;
  if (deserializeJson(respDoc, respBody) != DeserializationError::Ok) return;

  // Adoption latch: the server says whether this device is assigned to a
  // vehicle in some org. Absent field (old server, error path) = no change.
  if (respDoc["adopted"].is<bool>()) {
    updateAdoptionFromServer(respDoc["adopted"].as<bool>());
  }

  const char* otaVer = respDoc["ota"]["version"] | "";
  const int otaSize = respDoc["ota"]["size"] | 0;
  const char* otaMd5 = respDoc["ota"]["md5"] | "";
  if (strlen(otaVer) > 0 && semverGt(String(otaVer), String(TRACKER_FIRMWARE_VERSION))) {
    Serial.print("OTA available: v"); Serial.println(otaVer);
    downloadAndApplyOta(String(otaVer), otaSize, String(otaMd5));
  }
  if (!respDoc["udp_age_s"].isNull()) {
    g_lastServerUdpAgeS = respDoc["udp_age_s"].as<long>();
  }
  if (!lastGpsQualityFix && !respDoc["wifi_location"]["lat"].isNull()) {
    const float wLat = respDoc["wifi_location"]["lat"].as<float>();
    const float wLon = respDoc["wifi_location"]["lon"].as<float>();
    if (!isnan(wLat) && !isnan(wLon) && wLat != 0.0f) {
      lastKnownLat = wLat;
      lastKnownLon = wLon;
      Serial.print("WiFi geo: "); Serial.print(wLat, 6);
      Serial.print(","); Serial.println(wLon, 6);
    }
  }
}

// Parse a "+SHREQ: \"POST\",<status>,<datalen>" URC. Returns false if it doesn't match.
static bool parseShreqUrc(const String& line, int& status, int& datalen) {
  const int c1 = line.indexOf(',');
  if (c1 < 0) return false;
  const int c2 = line.indexOf(',', c1 + 1);
  if (c2 < 0) return false;
  status = line.substring(c1 + 1, c2).toInt();
  datalen = line.substring(c2 + 1).toInt();
  return status > 0;
}

// Read the response body via AT+SHREAD=0,<datalen>. Returns "" on failure.
static String shReadBody(int datalen) {
  drainSerialAt();
  SerialAT.print("AT+SHREAD=0,");
  SerialAT.print(datalen);
  SerialAT.print("\r\n");
  String hdr;
  if (!waitForLineContaining("+SHREAD:", hdr, 10000)) return "";
  const int colon = hdr.indexOf(':');
  const int len = (colon >= 0) ? hdr.substring(colon + 1).toInt() : 0;
  if (len <= 0) return "";
  String data;
  data.reserve(len + 1);
  uint32_t deadline = millis() + 10000;
  while ((int)data.length() < len && millis() < deadline) {
    while (SerialAT.available() && (int)data.length() < len) {
      data += (char)SerialAT.read();
    }
    delay(5);
  }
  return data;
}

// Native HTTPS POST via the SIM7000G's built-in HTTP(S) app (AT+SHREQ). The raw TLS
// sockets used by postJsonRawTls() drop ~half of HTTPS responses at this cadence (a
// documented modem limit); the native client handles the request/response framing in
// the modem and is far more reliable. Returns true only on a 2xx. Always tears down the
// SH session so a later raw-TLS/OTA CAOPEN isn't left fighting an open SH connection.
bool postJsonNative(const String& body) {
  const int sig = modem.getSignalQuality();
  if (sig == 0 || sig == 99) {
    Serial.print("Signal too weak ("); Serial.print(sig); Serial.println("), skipping");
    return false;
  }
  connectGprs();
  if (!gprsConnected) return false;

  CRUMB("shConn");
  sendSimpleAt("+SHDISC", 3000);  // drop any stale session first

  // SSL context for the SH app: TLS 1.2, no CA verification.
  sendSimpleAt("+CSSLCFG=\"sslversion\",1,3", 3000);
  sendSimpleAt("+SHSSL=1,\"\"", 3000);
  sendSimpleAt("+SHCONF=\"URL\",\"https://" + String(TRACKER_SERVER_HOST) + "\"", 3000);
  sendSimpleAt("+SHCONF=\"BODYLEN\",1024", 3000);
  sendSimpleAt("+SHCONF=\"HEADERLEN\",350", 3000);

  if (!sendSimpleAt("+SHCONN", TRACKER_TLS_CONNECT_TIMEOUT_S * 1000UL)) {
    Serial.println("SHCONN failed");
    sendSimpleAt("+SHDISC", 3000);
    return false;
  }

  sendSimpleAt("+SHCHEAD", 3000);  // clear any prior headers
  sendSimpleAt("+SHAHEAD=\"Content-Type\",\"application/json\"", 3000);
  sendSimpleAt("+SHAHEAD=\"x-tracker-key\",\"" + trackerApiKey() + "\"", 3000);
  sendSimpleAt("+SHAHEAD=\"User-Agent\",\"" + trackerDeviceId() + "/" +
               TRACKER_FIRMWARE_VERSION + "\"", 3000);

  // Body: AT+SHBOD=<len>,<timeout_ms> -> ">" prompt -> raw bytes -> OK.
  drainSerialAt();
  SerialAT.print("AT+SHBOD=");
  SerialAT.print(body.length());
  SerialAT.print(",10000\r\n");
  if (!waitForPromptChar('>', 5000)) {
    Serial.println("SHBOD: no prompt");
    sendSimpleAt("+SHDISC", 3000);
    return false;
  }
  SerialAT.print(body);
  if (!waitForOk(10000)) {
    Serial.println("SHBOD: body not accepted");
    sendSimpleAt("+SHDISC", 3000);
    return false;
  }

  // Request: type 3 = POST. Returns OK, then an async +SHREQ: "POST",<status>,<datalen>.
  CRUMB("shReq");
  drainSerialAt();
  SerialAT.print("AT+SHREQ=\"");
  SerialAT.print(TRACKER_SERVER_PATH);
  SerialAT.print("\",3\r\n");
  String shreqLine;
  if (!waitForLineContaining("+SHREQ:", shreqLine, 20000)) {
    Serial.println("SHREQ: no response");
    sendSimpleAt("+SHDISC", 3000);
    return false;
  }
  int status = 0, datalen = 0;
  parseShreqUrc(shreqLine, status, datalen);
  Serial.print("SHREQ status="); Serial.print(status);
  Serial.print(" datalen="); Serial.println(datalen);

  String respBody;
  if (datalen > 0) {
    CRUMB("shRead");
    respBody = shReadBody(datalen);
  }
  sendSimpleAt("+SHDISC", 3000);

  const bool success = (status >= 200 && status < 300);
  if (success && respBody.length() > 0) handleTelemetryResponse(respBody);
  return success;
}

// Raw-TLS POST (legacy path). Kept as the fallback for when the native SHREQ path is
// unsupported/misbehaving on a given modem firmware — see postJson() below.
bool postJsonRawTls(const String& body) {
  // Skip if signal is too weak — timeouts corrupt modem state.
  const int sig = modem.getSignalQuality();
  if (sig == 0 || sig == 99) {
    Serial.print("Signal too weak ("); Serial.print(sig); Serial.println("), skipping");
    return false;
  }

  connectGprs();
  if (!gprsConnected) return false;

  String request;
  request.reserve(body.length() + 300);
  request += "POST "; request += TRACKER_SERVER_PATH;
  request += " HTTP/1.1\r\nHost: "; request += TRACKER_SERVER_HOST_HEADER;
  request += "\r\nUser-Agent: "; request += trackerDeviceId();
  request += "/"; request += TRACKER_FIRMWARE_VERSION;
  request += "\r\nx-tracker-key: "; request += trackerApiKey();
  request += "\r\nContent-Type: application/json\r\nConnection: keep-alive\r\nContent-Length: ";
  request += body.length();
  request += "\r\n\r\n";
  request += body;

  // NOTE: the SIM7000G's raw TLS sockets drop ~half of HTTPS responses at this cadence
  // in clustered ~20-60s windows, regardless of fresh-vs-reuse/timing/retries (all
  // measured). The real fix is the modem's native HTTPS client (AT+SHREQ); until then
  // we keep this simple fresh-socket post and lean on the escalating auto-restart in
  // loop() to self-heal the sustained freezes.
  if (!ensureClientConnected()) return false;

  secureClient.print(request);

  CRUMB("httpRead");
  String response;
  response.reserve(512);
  uint32_t deadline = millis() + 8000;
  while (millis() < deadline) {
    while (secureClient.available()) {
      response += (char)secureClient.read();
      deadline = millis() + 2000;
    }
    if (!secureClient.connected() && !secureClient.available()) break;
    delay(10);
  }
  secureClient.stop();  // fresh socket next post

  if (response.length() == 0) {
    Serial.println("HTTP: no response");
    return false;
  }

  const bool success = response.indexOf(" 2") > 0;
  Serial.print("HTTP: ");
  Serial.println(response.substring(0, response.indexOf('\r')));

  // Parse JSON body (after blank line separating headers from body).
  const int bodyStart = response.indexOf("\r\n\r\n");
  if (success && bodyStart >= 0) {
    handleTelemetryResponse(response.substring(bodyStart + 4));
  }
  return success;
}

// Telemetry POST dispatcher. Tries the native SIM7000G HTTPS client (AT+SHREQ) first —
// it's far more reliable than raw TLS sockets — and transparently falls back to the
// raw-TLS path on failure. After repeated native failures it latches native OFF (until
// reboot) so a modem firmware that doesn't support SHREQ never keeps paying the failed
// attempt nor takes the fleet dark.
bool postJson(const String& body) {
#if TRACKER_USE_NATIVE_HTTPS
  static bool nativeEnabled = true;
  static uint8_t nativeFailStreak = 0;
  if (nativeEnabled) {
    if (postJsonNative(body)) {
      nativeFailStreak = 0;
      return true;
    }
    if (++nativeFailStreak >= 3) {
      Serial.println("Native HTTPS failing repeatedly — latching OFF, using raw TLS");
      nativeEnabled = false;
    } else {
      Serial.println("Native HTTPS failed — falling back to raw TLS for this post");
    }
    sendSimpleAt("+SHDISC", 3000);  // ensure no SH session lingers before raw CAOPEN
  }
#endif
  return postJsonRawTls(body);
}

// Standalone OTA check via TinyGsmClientSecure GET — independent of telemetry POST response.
void checkOtaStandalone() {
  Serial.println("OTA check: connecting...");
  const int sig = modem.getSignalQuality();
  if (sig == 0 || sig == 99) { Serial.println("OTA check: signal too weak"); return; }
  connectGprs();
  if (!gprsConnected) return;
  if (!ensureClientConnected()) return;

  const String req = String("GET /api/fleet/ota/check?version=") + TRACKER_FIRMWARE_VERSION +
                     "&k=" + trackerApiKey() +
                     " HTTP/1.1\r\nHost: " + TRACKER_SERVER_HOST_HEADER +
                     "\r\nUser-Agent: " + trackerDeviceId() + "/" + TRACKER_FIRMWARE_VERSION +
                     "\r\nConnection: close\r\n\r\n";
  secureClient.print(req);

  String response;
  response.reserve(512);
  uint32_t deadline = millis() + 15000;
  while (millis() < deadline) {
    while (secureClient.available()) {
      response += (char)secureClient.read();
      deadline = millis() + 2000;
    }
    if (!secureClient.connected() && !secureClient.available()) break;
    delay(10);
  }
  secureClient.stop();  // Connection: close — drop so next postJson() reconnects cleanly

  if (response.length() == 0) { Serial.println("OTA check: no response"); return; }

  const int jsonStart = response.indexOf('{');
  if (jsonStart < 0) return;
  String body = response.substring(jsonStart);

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok) return;
  const bool available = doc["available"] | false;
  const char* version = doc["version"] | "";
  const int size = doc["size"] | 0;
  const char* md5 = doc["md5"] | "";
  if (available && strlen(version) > 0 && semverGt(String(version), String(TRACKER_FIRMWARE_VERSION))) {
    Serial.print("OTA check: update available v"); Serial.println(version);
    downloadAndApplyOta(String(version), size, String(md5));
  } else {
    Serial.println("OTA check: up to date");
  }
}

void flushQueue() {
  while (queueCount > 0) {
    String body;
    if (!dequeue(body)) {
      return;
    }
    if (!postJson(body)) {
      enqueue(body);
      return;
    }
  }
}

// compact=true builds a trimmed payload for the 508-byte 1NCE UDP datagram limit:
// diagnostics-only fields are dropped (they still ship with every HTTPS heartbeat,
// which sends the full payload) and the wifi scan is capped. Position, motion,
// events, battery and the geolocation inputs the server needs are all kept.
String buildTelemetry(bool compact = false) {
  float lat = NAN;
  float lon = NAN;
  float speedKph = NAN;
  float altitudeM = NAN;
  int visibleSatellites = 0;
  int usedSatellites = 0;
  float accuracyM = NAN;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  // Drain accumulated modem URCs before querying GPS to avoid stale/corrupt responses.
  drainSerialAt();
  const bool hasFix = modem.getGPS(&lat, &lon, &speedKph, &altitudeM, &visibleSatellites,
                                   &usedSatellites, &accuracyM, &year, &month, &day,
                                   &hour, &minute, &second);
  // Quality filter: HDOP (filled into accuracyM by TinyGSM via CGNSINF) above 2.5 or
  // fewer than 4 used satellites → marginal fix; still reported but not treated as authoritative.
  bool qualityFix = hasFix && usedSatellites >= 4 &&
      (isnan(accuracyM) || accuracyM <= 2.5f);
  if (hasFix && !qualityFix) {
    Serial.print("GNSS: marginal fix — sats="); Serial.print(usedSatellites);
    Serial.print(" HDOP="); Serial.println(accuracyM);
  }
  // Outlier rejection: a sudden position jump that implies impossible speed is a bad fix.
  if (qualityFix && !isnan(prevFixLat) && lastFixMs > 0) {
    const float elapsedS = static_cast<float>(millis() - lastFixMs) / 1000.0f;
    if (elapsedS > 0.0f && elapsedS < 60.0f) {
      const float dlat = lat - prevFixLat;
      const float dlon = (lon - prevFixLon) * cos(radians(lat));
      const float distM = sqrtf(dlat * dlat + dlon * dlon) * 111111.0f;
      const float impliedKph = (distM / elapsedS) * 3.6f;
      if (impliedKph > 300.0f) {
        Serial.print("GNSS: outlier rejected — implied "); Serial.print(impliedKph, 0); Serial.println(" kph");
        qualityFix = false;
      }
    }
  }
  lastGpsFix = hasFix;
  lastGpsQualityFix = qualityFix;
  if (hasFix && !isnan(lat) && !isnan(lon)) {
    if (qualityFix) {
      if (gpsFirstFixMs == 0) gpsFirstFixMs = millis();
      // Compute course from two consecutive quality fixes when moving fast enough
      // for the position delta to be larger than GPS noise (~5m).
      if (!isnan(prevFixLat) && !isnan(prevFixLon) && !isnan(speedKph) && speedKph >= 8.0f) {
        const float dLon = radians(lon - prevFixLon);
        const float rlat1 = radians(prevFixLat);
        const float rlat2 = radians(lat);
        const float y = sin(dLon) * cos(rlat2);
        const float x = cos(rlat1) * sin(rlat2) - sin(rlat1) * cos(rlat2) * cos(dLon);
        lastKnownCourse = fmod(degrees(atan2(y, x)) + 360.0f, 360.0f);
      }
      prevFixLat = lat;
      prevFixLon = lon;
      lastFixMs = millis();
      lastKnownLat = lat;
      lastKnownLon = lon;
      // Persist position to NVS every 5 min for warm-start injection on next reboot.
      static uint32_t lastNvsSaveMs = 0;
      if (year > 0 && (lastNvsSaveMs == 0 || millis() - lastNvsSaveMs >= 300000UL)) {
        lastNvsSaveMs = millis();
        saveGnssPosition(lat, lon, altitudeM, year, month, day, hour, minute, second);
      }
    }
    if (year > 0) {
      snprintf(lastKnownTimestamp, sizeof(lastKnownTimestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
               year, month, day, hour, minute, second);
    }
  }

  // Stationary position lock: once stopped for STATIONARY_LOCK_MS, freeze lat/lon
  // to eliminate GPS multipath drift while parked. Reset when speed picks up.
  if (qualityFix) {
    if (!isnan(speedKph) && speedKph < STATIONARY_SPEED_KPH) {
      if (stationaryStartMs == 0) stationaryStartMs = millis();
      if (millis() - stationaryStartMs >= STATIONARY_LOCK_MS) {
        if (isnan(stationaryLat)) { stationaryLat = lat; stationaryLon = lon; }
        lat = stationaryLat;
        lon = stationaryLon;
      }
    } else {
      stationaryStartMs = 0;
      stationaryLat = NAN;
      stationaryLon = NAN;
    }
  }

  // WiFi scan for location fallback. CRITICAL: turning on the ESP32 WiFi radio
  // emits 2.4GHz RF + spikes current right next to the GPS front-end, which
  // desensitises the receiver and knocks out the weak-signal acquisition GPS
  // needs an uninterrupted window to complete. So:
  //  • While GPS has NOT yet achieved its first fix, give it a long clean
  //    runway — do NOT scan WiFi for the first 5 min, then only every 5 min.
  //  • Once GPS has a fix, WiFi scanning is harmless (used only as backup).
  String wifiScanJson;
  static uint32_t lastWifiScanMs = 0;
  const uint32_t nowMsWifi = millis();
  const bool gpsAcquiring = !lastGpsQualityFix && gpsEnabled;
  const uint32_t gnssRunMs = gpsEnabledAtMs ? (nowMsWifi - gpsEnabledAtMs) : 0;
  // Suppress WiFi entirely for the first 5 min of GPS acquisition.
  const bool suppressForGpsAcq = gpsAcquiring && gnssRunMs < 300000UL;
  // Only scan WiFi when we DON'T have a quality GPS fix — it's purely a geolocation
  // fallback. WiFi.scanNetworks() is blocking and can hang the WHOLE loop (the
  // mid-drive freeze: stuck with no posts, no recovery, no reboot). With a good fix
  // it's unneeded, so skip it entirely while driving.
  if (qualityFix) lastWifiScanMs = nowMsWifi;  // keep timer fresh so we don't scan the instant a fix drops
  if (!qualityFix && !suppressForGpsAcq && (lastWifiScanMs == 0 || nowMsWifi - lastWifiScanMs >= 300000UL)) {
    lastWifiScanMs = nowMsWifi;
    wifiScanJson = scanWifi();
    drainSerialAt(); // clear URCs that piled up during the blocking WiFi scan
  }

  const uint32_t nowMs = millis();
  const String eventName = qualityFix ? classifyEvent(speedKph, nowMs) : "";

  int16_t rssi = modem.getSignalQuality();
  uint16_t batteryMv = modem.getBattVoltage();
  g_lastBattMv = batteryMv;

  // No alternator-voltage sense on this hardware: a 12V→5V buck powers the board and
  // AT+CBC (batteryMv) only reads the buffer LiPo (~3.8V), so "ignition" can never be
  // derived from voltage. Derive trip state from GPS motion instead — moving now, or
  // moved within the grace window, counts as on-a-trip, so stop-and-go (lights/traffic)
  // doesn't collapse to the slow parked cadence. batteryMv is still reported for health.
  const bool wasIgnitionOn = ignitionOn;
  if (!isnan(speedKph) && speedKph >= TRACKER_MIN_MOVING_SPEED_KPH) {
    lastMovingMs = millis();
  }
  ignitionOn = (lastMovingMs != 0) && (millis() - lastMovingMs <= TRACKER_TRIP_GRACE_MS);
  // Track how long the trip's been over so nextIntervalMs() can drop to a heartbeat.
  if (ignitionOn) {
    ignitionOffSinceMs = 0;
  } else if (wasIgnitionOn || ignitionOffSinceMs == 0) {
    ignitionOffSinceMs = millis();
  }

  // Collect cell tower identity when no quality fix — sent to server for geolocation.
  CellInfo cellInfo;
  if (!qualityFix) {
    drainSerialAt();
    cellInfo = getCellInfo();
    drainSerialAt();
  }

  JsonDocument doc;
  doc["device_id"] = trackerDeviceId();
  doc["firmware"] = TRACKER_FIRMWARE_VERSION;
  doc["has_fix"] = hasFix;
  doc["fix_source"] = qualityFix ? "GPS" : (hasFix ? "GPS_MARGINAL" : "None");
  const String motion = hasFix ? motionState(speedKph) : String("unknown");
  doc["motion_state"] = motion;
  // Exposed so the loop can detect parked↔moving transitions and send those
  // reports over reliable HTTPS instead of fire-and-forget UDP.
  strlcpy(g_lastBuiltMotion, motion.c_str(), sizeof(g_lastBuiltMotion));
  doc["ignition_on"] = ignitionOn;
  doc["cell_rssi"] = rssi;
  doc["battery_mv"] = batteryMv;
  doc["queued_messages"] = queueCount;
  doc["boot_count"] = g_bootCount;  // cheap + tracks brownout resets — kept even compact
#if TRACKER_USE_NCE_UDP
  // UDP health diagnostics — visible in the app's device.raw without a serial
  // cable. fail_streak >0 = local CAOPEN/CASEND failures (DNS/session);
  // udp_cooldown = server-confirmed blackhole latch is active.
  if (g_udpFailStreak > 0) doc["udp_fail_streak"] = g_udpFailStreak;
  if ((int32_t)(millis() - g_udpDisabledUntilMs) < 0) doc["udp_cooldown"] = true;
#endif
  if (!compact) {
    doc["hardware_id"] = trackerHardwareId();
    doc["uptime_ms"] = nowMs;
    // GNSS diagnostics — always reported so we can tell "blind" (0 visible =
    // antenna/RF problem) from "searching" (visible>0 but not enough used = cold
    // start / weak sky). Also report how long GNSS has been enabled this boot.
    doc["sats_visible"] = visibleSatellites;
    doc["sats_used"] = usedSatellites;
    doc["gnss_on_ms"] = gpsEnabled && gpsEnabledAtMs ? (nowMs - gpsEnabledAtMs) : 0;
    doc["gnss_enabled"] = gpsEnabled;
    // Raw CGNSINF string — lets us read C/N0 (signal strength). C/N0=0 with 0 sats
    // = no RF reaching the receiver (antenna); C/N0>0 = signal present, locking issue.
    if (!qualityFix) {
      String graw = modem.getGPSraw();
      if (graw.length() > 0) doc["gnss_raw"] = graw;
    }
    if (gpsFirstFixMs > 0 && gpsEnabledAtMs > 0) {
      doc["ttff_ms"] = gpsFirstFixMs - gpsEnabledAtMs;
    }
    // Connectivity diagnostics — let the server show what happened during a mid-drive
    // stall without a serial cable. boot_count jumps on a brownout/crash reset;
    // watchdog/forced_reconnect counts climb when the data session dies (handovers);
    // free_heap falling over time would point at a memory leak.
    doc["free_heap"] = ESP.getFreeHeap();
    doc["watchdog_count"] = g_watchdogCount;
    doc["forced_reconnects"] = g_forcedReconnects;
    doc["reset_reason"] = g_resetReason;                       // why it last reset
    if (g_lastHangOp[0]) doc["last_hang_op"] = g_lastHangOp;   // what it was doing when it hung
  }

  if (eventName.length() > 0) {
    doc["event"] = eventName;
  }

  JsonObject gps = doc["gps"].to<JsonObject>();
  if (hasFix) {
    gps["lat"] = serialized(String(lat, 6));
    gps["lon"] = serialized(String(lon, 6));
    gps["speed_kph"] = serialized(String(speedKph, 2));
    gps["altitude_m"] = serialized(String(altitudeM, 2));
    gps["visible_satellites"] = visibleSatellites;
    gps["used_satellites"] = usedSatellites;
    if (!isnan(accuracyM)) {
      gps["accuracy_m"] = serialized(String(accuracyM, 2));
    }
    if (!isnan(lastKnownCourse)) {
      gps["course_deg"] = serialized(String(lastKnownCourse, 1));
    }
    if (year > 0) {
      char timestamp[25];
      snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
               year, month, day, hour, minute, second);
      gps["timestamp"] = timestamp;
    }
  }

  if (!qualityFix && !isnan(lastKnownLat) && !isnan(lastKnownLon)) {
    JsonObject lastGps = doc["last_gps"].to<JsonObject>();
    lastGps["lat"] = serialized(String(lastKnownLat, 6));
    lastGps["lon"] = serialized(String(lastKnownLon, 6));
    if (lastKnownTimestamp[0] != '\0') {
      lastGps["time"] = lastKnownTimestamp;
    }
  }

  if (!qualityFix && wifiScanJson.length() > 2) { // "[]" is 2 chars — only add if networks found
    JsonDocument wifiDoc;
    if (deserializeJson(wifiDoc, wifiScanJson) == DeserializationError::Ok) {
      // Compact (UDP) payloads must fit 508 bytes — 4 APs is plenty for the server's
      // WiFi geolocation; the full scan still goes out with every HTTPS heartbeat.
      if (compact) {
        JsonArray arr = wifiDoc.as<JsonArray>();
        while (arr.size() > 4) arr.remove(arr.size() - 1);
      }
      doc["wifi_scan"] = wifiDoc;
    }
  }

  // Dead reckoning: extrapolate last known position using speed + course.
  // Only valid for ≤5 min and ≤5 km — beyond that, drift makes it unreliable.
  if (!qualityFix && !isnan(lastKnownLat) && !isnan(lastKnownLon) &&
      !isnan(lastKnownCourse) && !isnan(lastSpeedKph) && lastSpeedKph >= 3.0f &&
      lastFixMs > 0) {
    const float elapsedSec = static_cast<float>(millis() - lastFixMs) / 1000.0f;
    if (elapsedSec > 0.0f && elapsedSec <= 300.0f) {
      const float distM = (lastSpeedKph / 3.6f) * elapsedSec;
      if (distM < 5000.0f) {
        const float hr = radians(lastKnownCourse);
        const float drLat = lastKnownLat + (distM / 111111.0f) * cos(hr);
        const float drLon = lastKnownLon +
            (distM / (111111.0f * cos(radians(lastKnownLat)))) * sin(hr);
        JsonObject dr = doc["dead_reckoning"].to<JsonObject>();
        dr["lat"] = serialized(String(drLat, 6));
        dr["lon"] = serialized(String(drLon, 6));
        dr["elapsed_s"] = static_cast<int>(elapsedSec);
        dr["course_deg"] = serialized(String(lastKnownCourse, 1));
      }
    }
  }

  if (!qualityFix && cellInfo.valid) {
    JsonObject ci = doc["cell_info"].to<JsonObject>();
    ci["radio"] = cellInfo.radioType;
    ci["mcc"] = cellInfo.mcc;
    ci["mnc"] = cellInfo.mnc;
    ci["lac"] = cellInfo.lac;
    ci["cid"] = cellInfo.cid;
  }

  String body;
  serializeJson(doc, body);
  return body;
}

#if TRACKER_USE_NCE_UDP
// Send one telemetry report as a plain UDP datagram to the 1NCE OS Device Integrator.
// No TLS: the packet never leaves the carrier network and the SIM is the identity.
// Uses the modem's CA app on cid 0 like the other transports; the OTA path showed this
// modem firmware selects the protocol via CASSLCFG (protocol,1 = UDP) with the 3-arg
// CAOPEN, so try that first and fall back to the documented 5-arg CAOPEN form.
// "Success" = the modem accepted the datagram — UDP has no delivery ACK; assured
// delivery comes from the periodic HTTPS heartbeat in postTelemetry().
bool postUdp1nce(const String& body) {
  if (body.length() > TRACKER_NCE_UDP_MAX_PAYLOAD) return false;
  const int sig = modem.getSignalQuality();
  if (sig == 0 || sig == 99) {
    Serial.print("UDP: signal too weak ("); Serial.print(sig); Serial.println("), skipping");
    return false;
  }
  connectGprs();
  if (!gprsConnected) return false;

  CRUMB("udpOpen");
  sendSimpleAt("+CACLOSE=0", 2000);          // cid 0 is shared with the TLS paths
  sendSimpleAt("+CASSLCFG=0,ssl,0", 2000);   // make sure no SSL config lingers on it
  sendSimpleAt("+CASSLCFG=0,protocol,1", 2000);  // 1 = UDP on this CA-app firmware

  // Hostname first, then the known 1NCE-internal IPs: some PDP sessions'
  // modem DNS can't resolve udp.os.1nce.com (field: every open failed and the
  // tracker burned TLS fallbacks all day).
  const char* targets[] = { TRACKER_NCE_UDP_HOST, TRACKER_NCE_UDP_IP1, TRACKER_NCE_UDP_IP2 };
  String line;
  bool opened = false;
  for (int t = 0; t < 3 && !opened; t++) {
    drainSerialAt();
    SerialAT.print("AT+CAOPEN=0,\""); SerialAT.print(targets[t]); SerialAT.print("\",");
    SerialAT.print(TRACKER_NCE_UDP_PORT); SerialAT.print("\r\n");
    opened = waitForLineContaining("+CAOPEN:", line, 15000);
    if (opened) {
      const int c = line.lastIndexOf(',');
      opened = c >= 0 && line.substring(c + 1).toInt() == 0;
    }
    if (!opened && t > 0) sendSimpleAt("+CACLOSE=0", 1000);
  }
  if (!opened) {
    // Some SIM7000 firmware revisions want the full documented form instead.
    drainSerialAt();
    SerialAT.print("AT+CAOPEN=0,0,\"UDP\",\"" TRACKER_NCE_UDP_HOST "\",");
    SerialAT.print(TRACKER_NCE_UDP_PORT); SerialAT.print("\r\n");
    opened = waitForLineContaining("+CAOPEN:", line, 15000);
    if (opened) {
      const int c = line.lastIndexOf(',');
      opened = c >= 0 && line.substring(c + 1).toInt() == 0;
    }
  }
  if (!opened) {
    Serial.println("UDP: CAOPEN failed");
    g_udpFailStreak++;
    sendSimpleAt("+CACLOSE=0", 2000);
    sendSimpleAt("+CASSLCFG=0,protocol,0", 2000);  // restore TCP for the TLS paths
    return false;
  }

  CRUMB("udpSend");
  drainSerialAt();
  SerialAT.print("AT+CASEND=0,"); SerialAT.print(body.length()); SerialAT.print("\r\n");
  if (!waitForPromptChar('>', 5000)) {
    Serial.println("UDP: CASEND no prompt");
    sendSimpleAt("+CACLOSE=0", 2000);
    sendSimpleAt("+CASSLCFG=0,protocol,0", 2000);  // restore TCP for the TLS paths
    return false;
  }
  SerialAT.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
  SerialAT.flush();
  const bool sent = waitForCasendComplete(10000);
  sendSimpleAt("+CACLOSE=0", 2000);
  // cid 0 is shared with the raw-TLS and OTA paths — leave it back on TCP so a
  // later CAOPEN from those paths doesn't inherit UDP mode.
  sendSimpleAt("+CASSLCFG=0,protocol,0", 2000);
  if (!sent) {
    Serial.println("UDP: CASEND not confirmed");
    g_udpFailStreak++;
  } else {
    g_udpFailStreak = 0;
    g_udpSentSinceHb++;
  }
  return sent;
}
#endif  // TRACKER_USE_NCE_UDP

// Transport dispatcher for one telemetry report. On UDP builds: a full-payload HTTPS
// POST every TRACKER_NCE_HTTPS_HEARTBEAT_MS (assured delivery, diagnostics, and the
// OTA offer comes back in its response); everything between heartbeats goes as one
// UDP datagram, falling back to HTTPS whenever UDP can't send or the payload can't
// fit the datagram. On normal builds this is just postJson().
bool postTelemetry(const String& body, bool forceReliable = false) {
#if TRACKER_USE_NCE_UDP
  static uint32_t lastHeartbeatMs = 0;
  const uint32_t now = millis();
  if (lastHeartbeatMs == 0 || now - lastHeartbeatMs >= TRACKER_NCE_HTTPS_HEARTBEAT_MS) {
    Serial.println("NCE: HTTPS heartbeat (full payload)");
    if (postJson(buildTelemetry(false))) {
      lastHeartbeatMs = now;
      // Blackhole detector: we sent plenty of datagrams since the last
      // heartbeat, yet the server's response says none have arrived recently
      // (or ever) — 1NCE isn't routing this session to the UDP endpoint.
      // Post via HTTPS for the cooldown instead of feeding a dead pipe.
      if (g_udpSentSinceHb >= 5 &&
          (g_lastServerUdpAgeS < 0 ||
           g_lastServerUdpAgeS * 1000L > (long)(TRACKER_NCE_HTTPS_HEARTBEAT_MS + 180000UL))) {
        g_udpDisabledUntilMs = now + TRACKER_NCE_UDP_COOLDOWN_MS;
        Serial.println("NCE: server sees no UDP arrivals — HTTPS-only for 60 min");
      }
      g_udpSentSinceHb = 0;
      return true;
    }
    // Heartbeat failed — fall through to UDP so live tracking continues; the
    // heartbeat (and its OTA check) retries on the next cycle.
  }
  // Motion-state transitions (parked↔moving) go over HTTPS: a lost UDP
  // datagram here leaves the map showing the WRONG state until the next
  // packet lands, which is exactly the "doesn't switch to moving" symptom.
  // Steady-state reports stay on cheap UDP. If HTTPS fails (dead zone), fall
  // through to UDP as a best-effort backup — the caller queues on false.
  if (forceReliable) {
    Serial.println("NCE: motion transition — reliable HTTPS post");
    if (postJson(body)) return true;
  }
  // Respect the blackhole cooldown — UDP sends would "succeed" locally and
  // never arrive, so go straight to HTTPS until it expires.
  const bool udpAllowed = (int32_t)(millis() - g_udpDisabledUntilMs) >= 0;
  if (udpAllowed && body.length() <= TRACKER_NCE_UDP_MAX_PAYLOAD && postUdp1nce(body)) return true;
  if (forceReliable) return false;  // already tried HTTPS above
  if (udpAllowed) Serial.println("NCE: UDP unavailable — HTTPS fallback for this post");
  return postJson(body);
#else
  (void)forceReliable;  // HTTPS is already reliable + ordered
  return postJson(body);
#endif
}

uint32_t nextIntervalMs() {
  if (!lastGpsQualityFix) return TRACKER_MOVING_INTERVAL_MS;  // seeking fix — stay fast
  if (isnan(lastSpeedKph) || lastSpeedKph < TRACKER_MIN_MOVING_SPEED_KPH) {
    if (ignitionOn) return 30000UL;  // idle engine: 30s
    // Parked: 2-min beacon normally; drop to a 10-min heartbeat once the engine's
    // been off a while (smart sleep). Still << the 35-min offline cutoff, so the map
    // keeps showing "parked".
    if (ignitionOffSinceMs != 0 && millis() - ignitionOffSinceMs >= DEEP_PARK_AFTER_MS) {
      return PARKED_HEARTBEAT_MS;
    }
    return TRACKER_PARKED_INTERVAL_MS;  // 2 min
  }
  // Adaptive: faster updates at highway speeds for accurate trip reconstruction.
  if (lastSpeedKph >= 80.0f) return 5000UL;   // highway: 5s
  if (lastSpeedKph >= 30.0f) return TRACKER_MOVING_INTERVAL_MS;  // city: 10s
  return 30000UL;                               // slow creep: 30s
}

// ─── NVS credential helpers ──────────────────────────────────────────────────

void loadCredentials() {
  Preferences prefs;
  prefs.begin("tracker", true);
  g_deviceId = prefs.getString("device_id", "");
  g_apiKey   = prefs.getString("api_key",   "");
  g_adopted  = prefs.getBool("adopted", false);
  prefs.end();
  g_provisioned = g_deviceId.length() > 0 && g_apiKey.length() > 0;
  if (g_provisioned) {
    Serial.print("Provisioned as: "); Serial.print(g_deviceId);
    Serial.println(g_adopted ? " (adopted)" : " (awaiting adoption — BLE claimable)");
  } else {
    Serial.println("Not provisioned — will self-provision from efuse MAC.");
  }
}

void saveCredentials(const String& deviceId, const String& apiKey) {
  Preferences prefs;
  prefs.begin("tracker", false);
  prefs.putString("device_id", deviceId);
  prefs.putString("api_key",   apiKey);
  prefs.end();
  g_deviceId    = deviceId;
  g_apiKey      = apiKey;
  g_provisioned = true;
}

void saveAdopted(bool adopted) {
  Preferences prefs;
  prefs.begin("tracker", false);
  prefs.putBool("adopted", adopted);
  prefs.end();
  g_adopted = adopted;
}

// Factory identity: tracker-<last 6 hex of the efuse MAC>. ESP.getEfuseMac()
// packs the 6 MAC octets little-endian (byte 0 = first octet), so octets
// 3..5 — the ones printed last by esptool during flashing — are bytes 3..5.
// flash_new_tracker.sh derives the same ID from esptool's MAC line, so the
// label printed at the flash station always matches what the board calls itself.
String factoryDeviceId() {
  const uint64_t mac = ESP.getEfuseMac();
  char id[20];
  snprintf(id, sizeof(id), "tracker-%02x%02x%02x",
           static_cast<uint8_t>(mac >> 24),
           static_cast<uint8_t>(mac >> 32),
           static_cast<uint8_t>(mac >> 40));
  return String(id);
}

// ─── SD-card self-flash ──────────────────────────────────────────────────────
// Field recovery/update without a laptop: put firmware.bin on a microSD card,
// insert, power-cycle. The board flashes it and reboots. An NVS marker with the
// file's MD5 prevents a re-flash loop (the same card can stay inserted); to
// force a re-apply of an identical binary, replace the file (any byte change)
// or erase NVS. Runs before anything network-related so even a unit whose
// modem/credential state is wrecked can be recovered.

void trySdFirmwareUpdate() {
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) { SPI.end(); return; }  // no card inserted — the normal case
  File f = SD.open("/firmware.bin", FILE_READ);
  if (!f || f.size() < 100000) {  // sanity floor: a real build is ~1MB+
    if (f) f.close();
    SD.end(); SPI.end();
    return;
  }
  Serial.print("SD: found /firmware.bin ("); Serial.print(f.size()); Serial.println(" bytes)");

  MD5Builder md5;
  md5.begin();
  static uint8_t buf[4096];
  size_t n;
  while ((n = f.read(buf, sizeof(buf))) > 0) md5.add(buf, n);
  md5.calculate();
  const String fileMd5 = md5.toString();

  Preferences prefs;
  prefs.begin("sdota", false);
  if (prefs.getString("applied", "") == fileMd5) {
    prefs.end(); f.close(); SD.end(); SPI.end();
    Serial.println("SD: firmware.bin already applied — skipping");
    return;
  }

  // Optional sidecar integrity check: firmware.md5 written by make_sd_card.sh.
  // A mismatch means the copy to the card was corrupted — refuse to flash it.
  File m = SD.open("/firmware.md5", FILE_READ);
  if (m) {
    String expect = m.readStringUntil('\n');
    m.close();
    expect.trim();
    if (expect.length() == 32 && !expect.equalsIgnoreCase(fileMd5)) {
      prefs.end(); f.close(); SD.end(); SPI.end();
      Serial.println("SD: firmware.bin MD5 mismatch vs firmware.md5 — corrupt copy, not flashing");
      return;
    }
  }

  Serial.println("SD: flashing new firmware...");
  f.seek(0);
  bool ok = Update.begin(f.size());
  if (ok) {
    ok = Update.writeStream(f) == f.size() && Update.end();
  }
  if (ok) {
    prefs.putString("applied", fileMd5);
    prefs.end(); f.close(); SD.end(); SPI.end();
    Serial.println("SD: flash complete — rebooting into new firmware");
    delay(200);
    ESP.restart();
  }
  Update.abort();
  prefs.end(); f.close(); SD.end(); SPI.end();
  Serial.print("SD: flash FAILED (error "); Serial.print(Update.getError());
  Serial.println(") — continuing boot on current firmware");
}

void updateBleDevInfo() {
  if (!g_bleDevInfoChar) return;
  char buf[96];
  snprintf(buf, sizeof(buf),
    "{\"device_id\":\"%s\",\"fw\":\"%s\",\"batt_mv\":%u}",
    trackerDeviceId().c_str(), TRACKER_FIRMWARE_VERSION, g_lastBattMv);
  g_bleDevInfoChar->setValue(std::string(buf));
}

bool g_presenceBleInited = false;  // bluedroid stack is up (deinit is unreliable — init once)
bool g_presenceBleOn = false;
BLEServer* g_bleServer = nullptr;
uint16_t g_bleConnId = 0;
uint32_t g_bleConnectedAtMs = 0;   // 0 = no central connected

// The stack stops advertising the moment a central connects (e.g. the app's
// presence watcher or claim scan reading devinfo) and does NOT resume it on
// disconnect — without this callback one background read from any phone made
// the tracker permanently invisible (bench-found: Add Device saw nothing
// because the owner's phone had already connected minutes earlier).
// onConnect stamps the connection so the loop can evict centrals that never
// leave (a leaked phone connection held a bench tracker hostage for a day —
// visible only as "Connected: 123Track" in the phone's Bluetooth settings).
class ReAdvertiseCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    g_bleConnId = s->getConnId();
    g_bleConnectedAtMs = millis();
  }
  void onDisconnect(BLEServer*) override {
    g_bleConnectedAtMs = 0;
    if (!g_adopted || g_presenceBleOn) {
      updateBleDevInfo();
      BLEDevice::startAdvertising();
    }
  }
};

void startBleAdvertising() {
  // The device identity rides IN the advertised name ("123T-<id suffix>") so
  // phones can identify trackers straight from the advertisement. The GATT
  // devinfo read hangs indefinitely on iOS against bluedroid (works from
  // macOS) — the app's scan found and connected to boards it then reported as
  // "no trackers found". Name-based identity needs no connection at all; the
  // devinfo characteristic stays for richer reads (fw/battery) by tolerant
  // clients.
  String suffix = g_deviceId.startsWith("tracker-") ? g_deviceId.substring(8) : g_deviceId;
  if (suffix.length() == 0) suffix = "unset";
  BLEDevice::init((String("123T-") + suffix).c_str());
  BLEServer*    server  = BLEDevice::createServer();
  g_bleServer = server;
  server->setCallbacks(new ReAdvertiseCallbacks());
  BLEService*   service = server->createService(FLEET_BLE_SERVICE_UUID);
  g_bleDevInfoChar = service->createCharacteristic(
    FLEET_BLE_DEVINFO_UUID, BLECharacteristic::PROPERTY_READ);
  updateBleDevInfo();
  service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(FLEET_BLE_SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  BLEDevice::startAdvertising();
  g_presenceBleInited = true;
  Serial.println("BLE: advertising as 123Track");
}

// ── Driver-presence advertising (adopted trackers) ───────────────────────────
// The phone app proves who's in the vehicle by hearing this tracker over BLE,
// so adopted units advertise while ON A TRIP WITH A GPS FIX. The trip-only
// restriction exists because BLE breaks WiFi.scanNetworks() — but WiFi-shortcut
// geolocation only runs when there's NO fix, so gating on a fix keeps that
// fallback intact: fix lost or trip over → advertising stops, radio freed.
void updatePresenceAdvertising(bool shouldAdvertise) {
  if (!g_adopted) return;  // unadopted boards already advertise continuously for claiming
  if (shouldAdvertise == g_presenceBleOn) return;
  if (shouldAdvertise) {
    if (!g_presenceBleInited) {
      startBleAdvertising();  // one-time stack init (bluedroid deinit is unreliable)
    } else {
      updateBleDevInfo();
      BLEDevice::startAdvertising();
    }
    Serial.println("BLE: presence advertising ON (trip + fix)");
  } else {
    BLEDevice::getAdvertising()->stop();
    Serial.println("BLE: presence advertising OFF");
  }
  g_presenceBleOn = shouldAdvertise;
}

// Server-driven adoption latch (see g_adopted). Called from every parsed
// telemetry/heartbeat response that carries an "adopted" field.
void updateAdoptionFromServer(bool adoptedNow) {
  if (adoptedNow == g_adopted) return;
  saveAdopted(adoptedNow);
  if (adoptedNow) {
    // Claimed in the app: stop the continuous setup advertising. From here on
    // updatePresenceAdvertising() owns the radio (trip + fix only).
    if (g_presenceBleInited) BLEDevice::getAdvertising()->stop();
    g_presenceBleOn = false;
    Serial.println("BLE: adopted by an account — setup advertising off");
  } else {
    // Un-claimed in the app: become adoptable again.
    if (g_presenceBleInited) {
      updateBleDevInfo();
      BLEDevice::startAdvertising();
    } else {
      startBleAdvertising();
    }
    g_presenceBleOn = false;
    Serial.println("BLE: un-adopted — advertising continuously for claiming");
  }
}

// Fires every 60s. If the loop heartbeat hasn't advanced across 3 checks (~180s),
// the main loop is hung — force a reboot. 180s clears the longest legit blocking
// call (waitForNetwork is 120s) so it won't false-trip.
void IRAM_ATTR onLoopWatchdog() {
  static uint32_t last = 0;
  static uint8_t misses = 0;
  if (g_loopBeat == last) {
    if (++misses >= 3) {
      esp_restart();
    }
  } else {
    misses = 0;
    last = g_loopBeat;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Fleet tracker booting");

  // Capture WHY we reset + WHAT we were doing when we hung (breadcrumb survives a
  // software/watchdog reset in RTC memory). Reported in telemetry to find the freeze.
  {
    const esp_reset_reason_t r = esp_reset_reason();
    switch (r) {
      case ESP_RST_POWERON:  g_resetReason = "poweron"; break;
      case ESP_RST_SW:       g_resetReason = "sw(watchdog)"; break;  // our timer ISR esp_restart()
      case ESP_RST_PANIC:    g_resetReason = "panic(crash)"; break;
      case ESP_RST_INT_WDT:  g_resetReason = "int_wdt"; break;
      case ESP_RST_TASK_WDT: g_resetReason = "task_wdt"; break;
      case ESP_RST_WDT:      g_resetReason = "other_wdt"; break;
      case ESP_RST_BROWNOUT: g_resetReason = "brownout(power)"; break;
      case ESP_RST_DEEPSLEEP: g_resetReason = "deepsleep"; break;
      default:               g_resetReason = "other"; break;
    }
    if (g_breadcrumbMagic == 0xB00B1E5UL && r != ESP_RST_POWERON) {
      strncpy(g_lastHangOp, g_breadcrumb, sizeof(g_lastHangOp) - 1);
      g_lastHangOp[sizeof(g_lastHangOp) - 1] = 0;
    }
    g_breadcrumbMagic = 0xB00B1E5UL;
    CRUMB("boot");
    Serial.print("Reset reason: "); Serial.print(g_resetReason);
    Serial.print(" | last op before reset: "); Serial.println(g_lastHangOp[0] ? g_lastHangOp : "(none)");
  }

  // Persisted boot counter — if this climbs in telemetry, the board is resetting
  // (brownout/crash) rather than just losing the data session.
  {
    Preferences prefs;
    prefs.begin("diag", false);
    g_bootCount = prefs.getULong("boots", 0) + 1;
    prefs.putULong("boots", g_bootCount);
    prefs.end();
    Serial.print("Boot count: "); Serial.println(g_bootCount);
  }

  // Hardware loop watchdog: reboot if the main loop hangs (timer 0, 1 MHz tick,
  // fires every 60s; reboots after ~180s of no loop progress).
  g_loopWdt = timerBegin(0, 80, true);
  timerAttachInterrupt(g_loopWdt, &onLoopWatchdog, true);
  timerAlarmWrite(g_loopWdt, 60000000ULL, true);
  timerAlarmEnable(g_loopWdt);

  // Field recovery/update: if a microSD card with a (new) firmware.bin is
  // inserted, flash it and reboot. Checked before anything else so a unit with
  // a wrecked modem or credential state can still be revived with just a card.
  trySdFirmwareUpdate();

  loadCredentials();
  if (!g_provisioned) {
    // First boot. Boards flashed with an explicit -D TRACKER_DEVICE_ID (the
    // fleet envs) persist those credentials; factory-image boards name
    // themselves from the efuse MAC. Either way the identity lives in NVS from
    // here on, so OTA updates never change it.
    if (String(TRACKER_DEVICE_ID) != "unprovisioned" && String(TRACKER_DEVICE_ID).length() > 0) {
      saveCredentials(String(TRACKER_DEVICE_ID), String(TRACKER_API_KEY));
      Serial.print("Provisioned from compile-time flags as: "); Serial.println(g_deviceId);
    } else {
      saveCredentials(factoryDeviceId(), String(TRACKER_API_KEY));
      Serial.print("Self-provisioned from efuse MAC as: "); Serial.println(g_deviceId);
    }
  }
  // Unadopted trackers advertise over BLE continuously so the app can claim
  // them (Devices → Add Device). Adopted trackers MUST NOT: BLE and WiFi share
  // the ESP32's 2.4GHz radio, and running BLE kills WiFi.scanNetworks() — the
  // location source when there's no GPS fix. Once the server's telemetry
  // response reports adopted:true this stops for good (see g_adopted), and
  // trip-time presence advertising takes over.
  if (!g_adopted) startBleAdvertising();

  powerOnModem();
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  if (!waitForModem()) {
    Serial.println("Modem unavailable; restarting in 30s");
    delay(30000);
    ESP.restart();
  }

  Serial.print("Modem: ");
  Serial.println(modem.getModemInfo());
  configureNetworkMode();

  logSimDiagnostics();
  connectGprs();
  // Power the GNSS receiver BEFORE pulling A-GPS, so AT+CAGPS injects EPO
  // predictions into a live engine (and they are no longer wiped by a reset).
  ensureGps();
  downloadAgps();
}

// ─── GPS ISOLATION TEST ──────────────────────────────────────────────────────
// Drops the cellular DATA session and streams the raw GNSS status to serial so we
// can see, directly off the chip, whether GPS acquires satellites with cellular
// quiet. If sats/C-N0 climb here but not in normal operation, the cellular data
// session was starving GPS. Remove this block to restore normal operation.
#define GPS_ISOLATION_TEST 0
#if GPS_ISOLATION_TEST
void loop() {
  maintainGps();   // keeps GNSS powered + configured (CGNSPWR/CGNSMOD/etc.)

  static bool dropped = false;
  if (!dropped) {
    Serial.println("\n=== GPS ISOLATION TEST: cellular data OFF, watching GNSS ===");
    modem.gprsDisconnect();          // drop the LTE data session
    dropped = true;
  }

  static uint32_t lastLog = 0;
  if (millis() - lastLog >= 3000) {
    lastLog = millis();
    drainSerialAt();
    String raw = modem.getGPSraw();  // AT+CGNSINF straight from the module
    // Parse: field 2 = fix, field 15 = sats-in-view, field 19 = C/N0 max.
    int fix = -1, sview = -1, cn0 = -1;
    {
      int idx = 0, start = 0;
      for (int i = 0; i <= (int)raw.length(); i++) {
        if (i == (int)raw.length() || raw[i] == ',') {
          String fld = raw.substring(start, i); start = i + 1;
          if (idx == 1 && fld.length()) fix = fld.toInt();
          if (idx == 14 && fld.length()) sview = fld.toInt();
          if (idx == 18 && fld.length()) cn0 = fld.toInt();
          idx++;
        }
      }
    }
    Serial.print("GPS["); Serial.print(millis() / 1000); Serial.print("s] fix=");
    Serial.print(fix); Serial.print(" sats_in_view="); Serial.print(sview);
    Serial.print(" C/N0="); Serial.print(cn0);
    Serial.print("  raw="); Serial.println(raw.length() ? raw : "<empty>");
  }

  delay(250);
}
#else
void loop() {
  g_loopBeat++;  // feed the hardware loop watchdog (reboots if this stops advancing)
  CRUMB("maintainGps");
  maintainGps();

  const uint32_t nowMs = millis();

  // Recovery is driven by consecutive POST failures below (interval-agnostic, so it
  // never fights the long parked-heartbeat interval the way a fixed-time watchdog did).

  // Refresh BLE device-info characteristic with latest battery reading every 30s.
  static uint32_t lastBleUpdateMs = 0;
  if (nowMs - lastBleUpdateMs >= 30000UL) {
    lastBleUpdateMs = nowMs;
    updateBleDevInfo();
  }

  // Driver-presence: advertise over BLE while on a trip with a GPS fix so the
  // driver's phone can identify this vehicle (see updatePresenceAdvertising).
  updatePresenceAdvertising(ignitionOn && lastGpsFix);

  // Unadopted boards must stay claimable: a WiFi scan (no-fix geolocation
  // fallback) silently kills BLE advertising and nothing re-arms it — bench
  // board advertised after every boot, went dark within minutes indoors, and
  // Add Device found nothing. Re-assert every 30s; a no-op while advertising
  // is already active.
  static uint32_t lastClaimAdvMs = 0;
  if (!g_adopted && g_presenceBleInited && millis() - lastClaimAdvMs >= 30000UL) {
    lastClaimAdvMs = millis();
    updateBleDevInfo();
    BLEDevice::startAdvertising();
  }

  // Evict centrals that connect and never leave: while a phone holds the GATT
  // link the tracker does not advertise AT ALL, so a leaky client silences it
  // for every scanner (field: a phone's stuck connection hid a bench board
  // for a day). Devinfo reads take ~2s; 45s means the other end leaked.
  if (g_bleConnectedAtMs != 0 && g_bleServer && millis() - g_bleConnectedAtMs >= 45000UL) {
    Serial.println("BLE: dropping stale central connection");
    g_bleServer->disconnect(g_bleConnId);
    g_bleConnectedAtMs = 0;
  }

  // Wake-on-motion: while parked the telemetry interval stretches to a 10-min
  // heartbeat. There's no alternator-voltage signal on this hardware, so poll GPS
  // speed directly every 15s; when the vehicle starts moving, jump straight to live
  // tracking instead of waiting out the heartbeat.
  static uint32_t lastWakeCheckMs = 0;
  if (!ignitionOn && nowMs - lastWakeCheckMs >= 15000UL) {
    lastWakeCheckMs = nowMs;
    CRUMB("wakeMotion");
    float wlat = NAN, wlon = NAN, wspd = NAN;
    if (modem.getGPS(&wlat, &wlon, &wspd) && !isnan(wspd) &&
        wspd >= TRACKER_MIN_MOVING_SPEED_KPH) {
      Serial.println("Motion detected while parked — waking to live tracking");
      ignitionOn = true;
      lastMovingMs = millis();
      ignitionOffSinceMs = 0;
      lastTelemetryMs = 0;  // post immediately
    }
  }

  // While posts are failing, retry fast — don't wait out a long parked-heartbeat
  // interval. When healthy, use the normal adaptive interval.
  const uint32_t interval = (g_consecutivePostFails > 0) ? 15000UL : nextIntervalMs();
  if (lastTelemetryMs == 0 || nowMs - lastTelemetryMs >= interval) {
    lastTelemetryMs = nowMs;
    CRUMB("buildTelem");
    // UDP builds send the compact payload (508-byte datagram limit); the full payload
    // still goes out with every HTTPS heartbeat inside postTelemetry().
    const String payload = buildTelemetry(TRACKER_USE_NCE_UDP != 0);

    // A parked↔moving change is the report the map can't afford to lose —
    // route it over reliable HTTPS instead of fire-and-forget UDP.
    static char lastSentMotion[12] = "";
    const bool motionChanged =
        lastSentMotion[0] != '\0' && strcmp(lastSentMotion, g_lastBuiltMotion) != 0;

    Serial.println(payload);
    CRUMB("postJson");
    const bool posted = postTelemetry(payload, motionChanged);
    if (posted) strlcpy(lastSentMotion, g_lastBuiltMotion, sizeof(lastSentMotion));
    if (!posted) {
      Serial.println("Queueing telemetry");
      enqueue(payload);
      g_consecutivePostFails++;
      // Escalating recovery — fastest action first. postJson already retries the SSL
      // socket internally, so reaching here means a real outage (dead PDP context, lost
      // registration, or a wedged modem). Each step does what a human would, ending in
      // the full restart that the field symptom needs — but automatically.
      if (g_consecutivePostFails == 3) {
        Serial.println("3 fails — fresh GPRS session");
        g_forcedReconnects++;
        modem.gprsDisconnect();
        gprsConnected = false;
        delay(500);
      } else if (g_consecutivePostFails == 8) {
        Serial.println("8 fails — power-cycling modem");
        g_watchdogCount++;
        modem.gprsDisconnect();
        delay(1000);
        powerOnModem();
        delay(5000);
        waitForModem();
        gpsEnabled = false;  // modem restart resets GNSS
        modem.gprsConnect(TRACKER_APN_PRIMARY, TRACKER_GPRS_USER, TRACKER_GPRS_PASS);
        gprsConnected = modem.isGprsConnected();
      } else if (g_consecutivePostFails >= 15) {
        Serial.println("15 fails — full reboot to recover (auto-restart)");
        delay(200);
        ESP.restart();
      }
    } else {
      lastPostSuccessMs = millis();
      g_consecutivePostFails = 0;
      CRUMB("flushQueue");
      flushQueue();
#if !TRACKER_USE_NCE_UDP
      // Every-5-posts OTA GET = a fresh TLS handshake every ~50s while driving — a big
      // slice of the SIM data burn. UDP builds get OTA offers in the HTTPS heartbeat
      // response instead (handleTelemetryResponse), so they skip this entirely.
      telemetryCycleCount++;
      if (telemetryCycleCount >= 5) {
        telemetryCycleCount = 0;
        CRUMB("otaCheck");
        checkOtaStandalone();
      }
#endif
    }
  }

  CRUMB("idle");
  delay(250);
}
#endif  // GPS_ISOLATION_TEST
