#pragma once

#include "rfid.h"

namespace App {

void begin();
void onRfidEvent(const RfidEvent& event);
void tick(uint32_t nowMs);

}  // namespace App
