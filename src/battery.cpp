#include "battery.h"

#include "config.h"

namespace Battery {

static float currentVoltage = 0.0f;  // volts
static bool lowFlag = false;
static bool chargingFlag = false;
static bool haveReading = false;
static uint32_t lastSampleMs = 0;

// Rolling history of readings, used to estimate the charge slope.
// Must hold at least BAT_CHARGE_WINDOW_MS / BAT_SAMPLE_INTERVAL_MS samples.
static const uint8_t HIST_SIZE = 128;
static float hist[HIST_SIZE];
static uint8_t histHead = 0;  // next write position
static uint8_t histCount = 0;

// Average several ADC samples to reduce noise, then undo the resistor divider.
static float readVoltage() {
  uint32_t accMv = 0;
  for (uint8_t i = 0; i < BAT_SAMPLE_COUNT; i++) {
    accMv += analogReadMilliVolts(BAT_ADC_PIN);
  }
  float pinMv = (float)accMv / (float)BAT_SAMPLE_COUNT;
  return pinMv * BAT_ADC_DIVIDER / 1000.0f;
}

// Least-squares slope (mV/min) of the most recent n history samples. A regression
// slope is used instead of a two-point difference because the raw ADC readings are
// noisy enough (several mV) to swamp the slow voltage rise of a charging battery.
static float chargeSlopeMvPerMin(uint8_t n) {
  if (n < 2) return 0.0f;

  uint8_t oldest = (histHead + HIST_SIZE - n) % HIST_SIZE;
  float sumX = 0.0f, sumY = 0.0f, sumXX = 0.0f, sumXY = 0.0f;
  for (uint8_t i = 0; i < n; i++) {
    float x = (float)i;
    float y = hist[(oldest + i) % HIST_SIZE] * 1000.0f;  // mV
    sumX += x;
    sumY += y;
    sumXX += x * x;
    sumXY += x * y;
  }

  float denom = sumXX - sumX * sumX / n;  // sum of squared x deviations
  if (denom <= 0.0f) return 0.0f;
  float slopePerSample = (sumXY - sumX * sumY / n) / denom;  // mV per sample
  return slopePerSample * 60000.0f / (float)BAT_SAMPLE_INTERVAL_MS;  // mV per minute
}

bool begin() {
  pinMode(BAT_ADC_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);

  currentVoltage = readVoltage();
  haveReading = true;
  lowFlag = currentVoltage < BAT_LOW_VOLTAGE;

  hist[0] = currentVoltage;
  histHead = 1;
  histCount = 1;
  lastSampleMs = millis();

  ESP_LOGI("BAT", "Battery: %.2f V (%d%%)%s", currentVoltage, percent(), lowFlag ? " LOW" : "");
  return true;
}

void tick(uint32_t nowMs) {
  if (haveReading && (nowMs - lastSampleMs) < BAT_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs = nowMs;

  currentVoltage = readVoltage();
  haveReading = true;
  lowFlag = currentVoltage < BAT_LOW_VOLTAGE;

  hist[histHead] = currentVoltage;
  histHead = (histHead + 1) % HIST_SIZE;
  if (histCount < HIST_SIZE) histCount++;

  // Charge detection: regression slope over the sampling window, with hysteresis
  // so ADC noise near the threshold does not toggle the flag on and off.
  uint8_t windowTicks = BAT_CHARGE_WINDOW_MS / BAT_SAMPLE_INTERVAL_MS;
  if (windowTicks > HIST_SIZE) windowTicks = HIST_SIZE;
  uint8_t n = histCount < windowTicks ? histCount : windowTicks;

  if (n >= BAT_CHARGE_MIN_SAMPLES) {
    float slope = chargeSlopeMvPerMin(n);
    bool wasCharging = chargingFlag;
    if (!chargingFlag) {
      if (slope >= BAT_CHARGE_SLOPE_MV_PER_MIN) chargingFlag = true;
    } else {
      if (slope < BAT_CHARGE_SLOPE_RELEASE_MV_PER_MIN) chargingFlag = false;
    }
    if (chargingFlag != wasCharging) {
      ESP_LOGI("BAT", "Charging: %s (slope %.1f mV/min over %u s)", chargingFlag ? "yes" : "no",
               slope, (unsigned)n * (BAT_SAMPLE_INTERVAL_MS / 1000));
    }
  }
}

float voltage() {
  return currentVoltage;
}

int percent() {
  float p = (currentVoltage - BAT_EMPTY_VOLTAGE) / (BAT_FULL_VOLTAGE - BAT_EMPTY_VOLTAGE) * 100.0f;
  if (p < 0.0f) p = 0.0f;
  if (p > 100.0f) p = 100.0f;
  return (int)(p + 0.5f);
}

bool low() {
  return lowFlag;
}

bool charging() {
  return chargingFlag;
}

}  // namespace Battery
