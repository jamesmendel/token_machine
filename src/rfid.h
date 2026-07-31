#pragma once

#include <Arduino.h>

enum class RfidEventType {
  None,
  Appeared,
  StillPresent,
  Disappeared,
};

struct RfidEvent {
  RfidEventType type;
  String uid;  // hex lowercase, valid for Appeared / StillPresent
};

namespace Rfid {

bool begin();
RfidEvent poll();
String lastUid();
bool cardPresent();

}  // namespace Rfid
