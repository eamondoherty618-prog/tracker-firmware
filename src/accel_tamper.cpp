#include "accel_tamper.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "tracker_config.h"

// ADXL375: fixed ±200 g range, 49 mg/LSB. Registers per datasheet.
namespace {

constexpr uint8_t REG_DEVID = 0x00;       // reads 0xE5
constexpr uint8_t REG_BW_RATE = 0x2C;     // 0x0A = 100 Hz
constexpr uint8_t REG_POWER_CTL = 0x2D;   // 0x08 = measure
constexpr uint8_t REG_DATAX0 = 0x32;      // 6 bytes X/Y/Z little-endian
constexpr float G_PER_LSB = 0.049f;

bool g_present = false;
uint8_t g_addr = TRACKER_ACCEL_ADDR;

struct ImpactEvent { uint32_t atMs; float peakG; };
constexpr size_t MAX_EVENTS = 5;
ImpactEvent g_events[MAX_EVENTS];
size_t g_eventCount = 0;
float g_sessionPeakG = 0;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(g_addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(g_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)g_addr, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void recordEvent(uint32_t atMs, float peakG) {
  portENTER_CRITICAL(&g_mux);
  if (g_eventCount == MAX_EVENTS) {                 // drop oldest
    for (size_t i = 1; i < MAX_EVENTS; i++) g_events[i - 1] = g_events[i];
    g_eventCount--;
  }
  g_events[g_eventCount++] = { atMs, peakG };
  if (peakG > g_sessionPeakG) g_sessionPeakG = peakG;
  portEXIT_CRITICAL(&g_mux);
}

// 100 Hz sampler on core 0 (Arduino loop lives on core 1). An "impact" is the
// dynamic magnitude (vector length minus 1 g of gravity) crossing the
// configured threshold; the peak over the following 300 ms window is what
// gets recorded, and a debounce keeps one pothole from logging five events.
void samplerTask(void*) {
  uint32_t lastEventMs = 0;
  uint32_t windowEndMs = 0;
  float windowPeak = 0;
#if TRACKER_BENCH_DEBUG
  uint32_t lastDebugMs = 0;
#endif

  for (;;) {
    uint8_t raw[6];
    if (readRegs(REG_DATAX0, raw, 6)) {
      const int16_t x = (int16_t)(raw[0] | (raw[1] << 8));
      const int16_t y = (int16_t)(raw[2] | (raw[3] << 8));
      const int16_t z = (int16_t)(raw[4] | (raw[5] << 8));
      const float gx = x * G_PER_LSB, gy = y * G_PER_LSB, gz = z * G_PER_LSB;
      const float dyn = fabsf(sqrtf(gx * gx + gy * gy + gz * gz) - 1.0f);
      const uint32_t now = millis();

#if TRACKER_BENCH_DEBUG
      if (now - lastDebugMs >= 500) {
        lastDebugMs = now;
        Serial.printf("ACCEL x=%.2f y=%.2f z=%.2f |dyn|=%.2f g\n", gx, gy, gz, dyn);
      }
#endif

      if (windowEndMs != 0) {                        // inside an event window
        if (dyn > windowPeak) windowPeak = dyn;
        if ((int32_t)(now - windowEndMs) >= 0) {
          recordEvent(now, windowPeak);
          Serial.printf("ACCEL: impact %.1f g\n", windowPeak);
          windowEndMs = 0;
          lastEventMs = now;
        }
      } else if (dyn >= TRACKER_IMPACT_THRESHOLD_G &&
                 now - lastEventMs >= TRACKER_IMPACT_DEBOUNCE_MS) {
        windowEndMs = now + 300;                     // capture the true peak
        windowPeak = dyn;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace

void accelInit() {
#if TRACKER_BENCH_DEBUG
  // Line check BEFORE claiming the pins: a wired-up I2C bus idles HIGH
  // (breakout pull-ups). HIGH/HIGH = wires land here, sensor just mute
  // (CS strap / dead sensor). LOW or drifting = wire, ground, or wrong pin.
  pinMode(TRACKER_ACCEL_SDA, INPUT);
  pinMode(TRACKER_ACCEL_SCL, INPUT);
  delay(5);
  Serial.printf("ACCEL: line check SDA(21)=%s SCL(22)=%s\n",
                digitalRead(TRACKER_ACCEL_SDA) ? "HIGH" : "LOW",
                digitalRead(TRACKER_ACCEL_SCL) ? "HIGH" : "LOW");
#endif

  Wire.begin(TRACKER_ACCEL_SDA, TRACKER_ACCEL_SCL);
  Wire.setTimeOut(50);

#if TRACKER_BENCH_DEBUG
  // Bench visibility: who is actually on the bus?
  Serial.print("ACCEL: I2C scan:");
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf(" 0x%02x", a);
  }
  Serial.println();
#endif

  // The ADXL375's ALT ADDRESS pin selects 0x53 (low) or 0x1D (high) — breakout
  // boards differ, so probe both rather than dictating the strap.
  uint8_t devid = 0;
  g_addr = TRACKER_ACCEL_ADDR;
  bool found = readRegs(REG_DEVID, &devid, 1) && devid == 0xE5;
  if (!found) {
    g_addr = 0x1D;
    found = readRegs(REG_DEVID, &devid, 1) && devid == 0xE5;
  }
  if (!found) {
    Serial.println("ACCEL: ADXL375 not present — continuing without impact detection");
    return;
  }
  Serial.printf("ACCEL: ADXL375 at 0x%02x\n", g_addr);
  if (!writeReg(REG_BW_RATE, 0x0A) || !writeReg(REG_POWER_CTL, 0x08)) {
    Serial.println("ACCEL: ADXL375 found but config failed — disabled");
    return;
  }
  g_present = true;
  xTaskCreatePinnedToCore(samplerTask, "accel", 3072, nullptr, 1, nullptr, 0);
  Serial.println("ACCEL: ADXL375 sampling at 100 Hz (impact threshold "
                 + String(TRACKER_IMPACT_THRESHOLD_G, 1) + " g)");
}

bool accelPresent() { return g_present; }

void accelConfigureWakeOnInt1() {
  // v1 stub. Future: map ACTIVITY interrupt to INT1 (0x2E/0x2F), then
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_34, 1) before esp_deep_sleep_start().
}

void accelAppendTelemetry(JsonDocument& doc, bool compact) {
  if (compact) {
    if (!g_present) return;
    // UDP budget: only when something recent happened, and only the peak.
    portENTER_CRITICAL(&g_mux);
    float recent = 0;
    const uint32_t now = millis();
    for (size_t i = 0; i < g_eventCount; i++) {
      if (now - g_events[i].atMs <= 10 * 60 * 1000UL && g_events[i].peakG > recent) recent = g_events[i].peakG;
    }
    portEXIT_CRITICAL(&g_mux);
    if (recent > 0) doc["impact_g"] = serialized(String(recent, 1));
    return;
  }

  JsonObject imp = doc["impact"].to<JsonObject>();
  imp["present"] = g_present;
  if (!g_present) return;
  portENTER_CRITICAL(&g_mux);
  const float sessionPeak = g_sessionPeakG;
  ImpactEvent events[MAX_EVENTS];
  const size_t n = g_eventCount;
  memcpy(events, g_events, sizeof(events));
  portEXIT_CRITICAL(&g_mux);

  if (sessionPeak > 0) imp["peak_g"] = serialized(String(sessionPeak, 1));
  if (n > 0) {
    const uint32_t now = millis();
    JsonArray ev = imp["events"].to<JsonArray>();
    for (size_t i = 0; i < n; i++) {
      JsonObject e = ev.add<JsonObject>();
      e["age_s"] = (now - events[i].atMs) / 1000;
      e["g"] = serialized(String(events[i].peakG, 1));
    }
  }
}
