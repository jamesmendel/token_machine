#include "rfid.h"

#include <Wire.h>
#include "MFRC522_I2C.h"
#include "config.h"

namespace Rfid {

static MFRC522_I2C mfrc(RFID_I2C_ADDR, RFID_RST_PIN);
static String currentUid;
static String lastSeenUid;
static bool present = false;
static uint8_t goneCount = 0;
static uint32_t lastPollMs = 0;

static String uidToHex(const MFRC522_I2C::Uid& uid) {
  String hex;
  hex.reserve(uid.size * 2);
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) hex += '0';
    hex += String(uid.uidByte[i], HEX);
  }
  hex.toLowerCase();
  return hex;
}

// Detect idle (REQA) or halted (WUPA) cards, then select + halt so next poll can re-detect.
static bool tryReadUid(String& uidOut) {
  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);

  byte result = mfrc.PICC_RequestA(bufferATQA, &bufferSize);
  if (result != MFRC522_I2C::STATUS_OK && result != MFRC522_I2C::STATUS_COLLISION) {
    bufferSize = sizeof(bufferATQA);
    result = mfrc.PICC_WakeupA(bufferATQA, &bufferSize);
  }
  if (result != MFRC522_I2C::STATUS_OK && result != MFRC522_I2C::STATUS_COLLISION) {
    return false;
  }
  if (!mfrc.PICC_ReadCardSerial()) {
    return false;
  }

  uidOut = uidToHex(mfrc.uid);
  mfrc.PICC_HaltA();
  return true;
}

bool begin() {
  Wire.begin(RFID_SDA_PIN, RFID_SCL_PIN);
  mfrc.PCD_Init();
  byte version = mfrc.PCD_ReadRegister(mfrc.VersionReg);
  ESP_LOGI("RFID", "MFRC522 Firmware Version: 0x%X", version);
  return version != 0x00 && version != 0xFF;
}

String lastUid() {
  return lastSeenUid;
}

bool cardPresent() {
  return present;
}

RfidEvent poll() {
  uint32_t now = millis();
  if (now - lastPollMs < RFID_POLL_INTERVAL_MS) {
    return {RfidEventType::None, ""};
  }
  lastPollMs = now;

  String uid;
  bool detected = tryReadUid(uid);

  if (detected) {
    lastSeenUid = uid;
    goneCount = 0;

    if (!present) {
      present = true;
      currentUid = uid;
      ESP_LOGI("RFID", "Appeared: %s", uid.c_str());
      return {RfidEventType::Appeared, uid};
    }

    if (!uid.equalsIgnoreCase(currentUid)) {
      currentUid = uid;
      ESP_LOGI("RFID", "Appeared (swap): %s", uid.c_str());
      return {RfidEventType::Appeared, uid};
    }

    return {RfidEventType::StillPresent, uid};
  }

  if (present) {
    goneCount++;
    if (goneCount >= RFID_GONE_DEBOUNCE_POLLS) {
      String gone = currentUid;
      present = false;
      currentUid = "";
      goneCount = 0;
      ESP_LOGI("RFID", "Disappeared: %s", gone.c_str());
      return {RfidEventType::Disappeared, gone};
    }
  }

  return {RfidEventType::None, ""};
}

}  // namespace Rfid
