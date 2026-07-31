#pragma once

#include <Arduino.h>

// RFID (MFRC522 over I2C)
static const uint8_t RFID_I2C_ADDR = 0x28;
static const int RFID_SDA_PIN = 16;
static const int RFID_SCL_PIN = 15;
static const int RFID_RST_PIN = 1;
static const uint8_t RFID_GONE_DEBOUNCE_POLLS = 4;
static const uint32_t RFID_POLL_INTERVAL_MS = 50;

// Session timeouts
static const uint32_t SESSION_COUNTDOWN_START_MS = 2000;  // show logout countdown
static const uint32_t SESSION_LOGOUT_MS = 5000;           // return to idle
static const uint32_t UNKNOWN_TAG_TIMEOUT_MS = 3000;

// SoftAP
static const char* WIFI_AP_SSID = "TokenMachine";
static const char* WIFI_AP_PASS = "tokenmachine";
static const uint8_t WIFI_AP_CHANNEL = 1;
static const uint8_t WIFI_AP_MAX_CONN = 4;

// Tag database (Preferences / NVS)
static const char* TAG_DB_NAMESPACE = "tags";
static const char* TAG_DB_KEY = "db";
static const size_t TAG_DB_MAX_JSON = 3072;
static const size_t TAG_NAME_MAX = 32;
static const size_t TAG_UID_MAX = 16;  // hex chars, null-terminated buffer = 17

// UI
static const uint8_t UI_ROTATION = 1;  // landscape 320x240
