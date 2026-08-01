#include <Arduino.h>

#include "app.h"
#include "audio.h"
#include "rfid.h"
#include "tag_db.h"
#include "ui.h"
#include "web.h"

static const char* LOGTAG = "SETUP";

void setup() {
  Serial.begin(115200);
  ESP_LOGI(LOGTAG, "------------------------------------------");
  ESP_LOGI(LOGTAG, "Chip: %s Rev %d", ESP.getChipModel(), ESP.getChipRevision());
  ESP_LOGI(LOGTAG, "CPU: %d MHz, %d cores", ESP.getCpuFreqMHz(), ESP.getChipCores());
  ESP_LOGI(LOGTAG, "Flash: %d KB, SDK: %s", ESP.getFlashChipSize() / 1024, ESP.getSdkVersion());
  ESP_LOGI(LOGTAG, "Free heap: %d bytes", ESP.getFreeHeap());
  ESP_LOGI(LOGTAG, "------------------------------------------");

  if (!TagDb::begin()) {
    ESP_LOGE(LOGTAG, "TagDb init failed");
  }
  Ui::begin();
  if (!Rfid::begin()) {
    ESP_LOGW(LOGTAG, "RFID init may have failed (bad version read)");
  }
  if (!Audio::begin()) {
    ESP_LOGW(LOGTAG, "Audio init failed (speaker disabled)");
  }
  if (!Web::begin()) {
    ESP_LOGE(LOGTAG, "Web SoftAP init failed");
  }
  App::begin();

  ESP_LOGI(LOGTAG, "Ready. SoftAP SSID: TokenMachine");
}

void loop() {
  RfidEvent ev = Rfid::poll();
  if (ev.type != RfidEventType::None) {
    App::onRfidEvent(ev);
  }
  App::tick(millis());
  Web::handleClient();
}
