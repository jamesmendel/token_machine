#pragma once

#include <Arduino.h>

namespace Battery {

bool begin();
void tick(uint32_t nowMs);

float voltage();   // battery voltage in volts
int percent();     // 0..100 state-of-charge estimate
bool low();        // true when below BAT_LOW_VOLTAGE
bool charging();   // true when a voltage rise suggests USB charging

}  // namespace Battery
