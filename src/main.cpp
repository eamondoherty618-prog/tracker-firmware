#include <Arduino.h>
#include <ArduinoJson.h>
#include <TinyGsmClient.h>

#include "tracker_config.h"

namespace {

// LilyGo T-SIM7000G v1.x defaults.
constexpr int MODEM_TX = 27;
constexpr int MODEM_RX = 26;
constexpr int MODEM_PWRKEY = 4;
constexpr int MODEM_BAUD = 115200;

constexpr size_t QUEUE_DEPTH = 24;

HardwareSerial SerialAT(1);
TinyGsm modem(SerialAT);

struct QueuedMessage {
  String body;
  uint32_t queuedAtMs = 0;
};

QueuedMessage queue[QUEUE_DEPTH];
size_t queueHead = 0;
size_t queueCount = 0;

uint32_t lastTelemetryMs = 0;
uint32_t lastSpeedSampleMs = 0;
float lastSpeedKph = NAN;
bool gpsEnabled = false;
bool gprsConnected = false;
uint32_t lastNetworkWaitMs = 0;
uint32_t gpsEnabledAtMs = 0;
uint32_t lastGnssStatusLogMs = 0;
bool lastGpsFix = false;

void powerOnModem() {
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(100);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1100);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(3000);
}

bool waitForModem() {
  Serial.print("Waiting for modem");
  for (int attempt = 0; attempt < 20; attempt++) {
    if (modem.testAT(1000)) {
      Serial.println(" ok");
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

  const bool connected = modem.waitForNetwork(120000L, true);
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

void ensureGps() {
  if (gpsEnabled) {
    return;
  }

  Serial.println("Enabling GNSS...");
  gpsEnabled = modem.enableGPS();
  Serial.println(gpsEnabled ? "GNSS enabled" : "GNSS enable failed");
  if (gpsEnabled) {
    gpsEnabledAtMs = millis();
    lastGnssStatusLogMs = 0;
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
    logGnssStatus();
  }

  if (!lastGpsFix && gpsEnabledAtMs != 0 &&
      nowMs - gpsEnabledAtMs >= TRACKER_GNSS_RECYCLE_MS) {
    restartGps();
  }
}

String motionState(float speedKph) {
  if (isnan(speedKph)) {
    return "unknown";
  }
  return speedKph >= TRACKER_MIN_MOVING_SPEED_KPH ? "moving" : "parked";
}

String trackerDeviceId() {
  return String(TRACKER_DEVICE_ID);
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

  if (dt <= TRACKER_EVENT_WINDOW_MS && delta <= TRACKER_HARD_BRAKE_DELTA_KPH) {
    return "hard_brake";
  }
  if (dt <= TRACKER_EVENT_WINDOW_MS && delta >= TRACKER_RAPID_ACCEL_DELTA_KPH) {
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

bool postJson(const String& body) {
  connectGprs();
  if (!gprsConnected) {
    return false;
  }

  sendSimpleAt("+CACLOSE=0", 2000);

  if (!sendSimpleAt("+CACID=0", 5000)) {
    Serial.println("CACID failed");
    return false;
  }
  if (!sendSimpleAt("+CSSLCFG=\"sslversion\",0,3", 5000)) {
    Serial.println("CSSLCFG sslversion failed");
    return false;
  }
  sendSimpleAt("+CSSLCFG=\"ignorertctime\",0,1", 5000);
  if (!sendSimpleAt("+CASSLCFG=0,ssl,1", 5000)) {
    Serial.println("CASSLCFG ssl failed");
    return false;
  }
  if (!sendAtAndWaitForToken("+CSSLCFG=\"ctxindex\",0", "+CSSLCFG:", 5000) ||
      !waitForOkVerbose(5000, "ctxindex")) {
    Serial.println("CSSLCFG ctxindex failed");
    return false;
  }
  if (!sendSimpleAt("+CASSLCFG=0,protocol,0", 5000)) {
    Serial.println("CASSLCFG protocol failed");
    return false;
  }
  if (!sendSimpleAt(String("+CSSLCFG=\"sni\",0,\"") + TRACKER_SERVER_HOST + "\"", 5000)) {
    Serial.println("CSSLCFG sni failed");
    return false;
  }

  String request;
  request.reserve(body.length() + 256);
  request += "POST ";
  request += TRACKER_SERVER_PATH;
  request += " HTTP/1.1\r\nHost: ";
  request += TRACKER_SERVER_HOST_HEADER;
  if (TRACKER_SERVER_PORT != 443) {
    request += ":";
    request += TRACKER_SERVER_PORT;
  }
  request += "\r\nUser-Agent: ";
  request += trackerDeviceId();
  request += "/";
  request += TRACKER_FIRMWARE_VERSION;
  request += "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ";
  request += body.length();
  request += "\r\n\r\n";
  request += body;

  String matchedLine;
  drainSerialAt();
  SerialAT.print("AT+CAOPEN=0,\"");
  SerialAT.print(TRACKER_SERVER_HOST);
  SerialAT.print("\",");
  SerialAT.print(TRACKER_SERVER_PORT);
  SerialAT.print("\r\n");
  if (!waitForLineContaining("+CAOPEN:", matchedLine, 75000)) {
    Serial.println("CAOPEN timeout");
    return false;
  }
  Serial.print("CAOPEN response: ");
  Serial.println(matchedLine);
  const int openComma = matchedLine.lastIndexOf(',');
  if (openComma < 0 || matchedLine.substring(openComma + 1).toInt() != 0) {
    return false;
  }

  drainSerialAt();
  SerialAT.print("AT+CASEND=0,");
  SerialAT.print(request.length());
  SerialAT.print("\r\n");
  if (!waitForPromptChar('>', 10000)) {
    Serial.println("CASEND prompt failed");
    return false;
  }

  SerialAT.write(reinterpret_cast<const uint8_t*>(request.c_str()), request.length());
  SerialAT.write(0x1A);
  SerialAT.flush();
  bool casendTimedOut = false;
  if (!waitForLineContaining("+CASEND:", matchedLine, 30000)) {
    Serial.println("CASEND response timeout");
    casendTimedOut = true;
  } else {
    Serial.print("CASEND response: ");
    Serial.println(matchedLine);

    const int firstComma = matchedLine.indexOf(',');
    const int secondComma = matchedLine.indexOf(',', firstComma + 1);
    const int sendResult =
        (firstComma >= 0 && secondComma > firstComma)
            ? matchedLine.substring(firstComma + 1, secondComma).toInt()
            : -1;
    const int sentBytes =
        (secondComma > firstComma) ? matchedLine.substring(secondComma + 1).toInt() : -1;
    if (sendResult != 0 || sentBytes != request.length()) {
      return false;
    }
  }

  delay(1500);
  drainSerialAt();
  SerialAT.print("AT+CARECV=0,512\r\n");
  if (!waitForLineContaining("+CARECV:", matchedLine, 15000)) {
    Serial.println("CARECV timeout");
    sendSimpleAt("+CACLOSE=0", 5000);
    if (casendTimedOut) {
      Serial.println("Assuming POST success after send/receive timeout");
      return true;
    }
    return false;
  }
  Serial.print("CARECV response: ");
  Serial.println(matchedLine);

  String statusLine;
  waitForLineContaining("HTTP/", statusLine, 5000);
  if (statusLine.length() > 0) {
    Serial.print("HTTP status: ");
    Serial.println(statusLine);
  }

  sendSimpleAt("+CACLOSE=0", 5000);
  if (statusLine.indexOf(" 2") > 0) {
    return true;
  }
  if (casendTimedOut) {
    Serial.println("Assuming POST success after CASEND timeout");
    return true;
  }
  return false;
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

String buildTelemetry() {
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

  const bool hasFix = modem.getGPS(&lat, &lon, &speedKph, &altitudeM, &visibleSatellites,
                                   &usedSatellites, &accuracyM, &year, &month, &day,
                                   &hour, &minute, &second);
  lastGpsFix = hasFix;
  const uint32_t nowMs = millis();
  const String eventName = hasFix ? classifyEvent(speedKph, nowMs) : "";

  int16_t rssi = modem.getSignalQuality();
  uint16_t batteryMv = modem.getBattVoltage();

  JsonDocument doc;
  doc["device_id"] = trackerDeviceId();
  doc["hardware_id"] = trackerHardwareId();
  doc["uptime_ms"] = nowMs;
  doc["firmware"] = TRACKER_FIRMWARE_VERSION;
  doc["has_fix"] = hasFix;
  doc["motion_state"] = hasFix ? motionState(speedKph) : "unknown";
  doc["cell_rssi"] = rssi;
  doc["battery_mv"] = batteryMv;
  doc["queued_messages"] = queueCount;

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
    if (year > 0) {
      char timestamp[25];
      snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
               year, month, day, hour, minute, second);
      gps["timestamp"] = timestamp;
    }
  }

  String body;
  serializeJson(doc, body);
  return body;
}

uint32_t nextIntervalMs() {
  if (isnan(lastSpeedKph) || lastSpeedKph >= TRACKER_MIN_MOVING_SPEED_KPH) {
    return TRACKER_MOVING_INTERVAL_MS;
  }
  return TRACKER_PARKED_INTERVAL_MS;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Fleet tracker prototype booting");

  powerOnModem();
  SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

  if (!waitForModem()) {
    Serial.println("Modem unavailable; restarting in 30s");
    delay(30000);
    ESP.restart();
  }

  Serial.print("Modem: ");
  Serial.println(modem.getModemInfo());
  logSimDiagnostics();
  configureNetworkMode();

  connectGprs();
  ensureGps();
}

void loop() {
  maintainGps();

  const uint32_t nowMs = millis();
  if (lastTelemetryMs == 0 || nowMs - lastTelemetryMs >= nextIntervalMs()) {
    lastTelemetryMs = nowMs;
    const String payload = buildTelemetry();

    Serial.println(payload);
    if (!postJson(payload)) {
      Serial.println("Queueing telemetry");
      enqueue(payload);
    } else {
      flushQueue();
    }
  }

  delay(250);
}
