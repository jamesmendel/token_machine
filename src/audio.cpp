#include "audio.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "config.h"
#include "es8311.h"

#include "audio_data/sound_login.h"
#include "audio_data/sound_token.h"

namespace Audio {

static const char* LOGTAG = "AUDIO";
static const size_t kChunkBytes = 512;

struct PlayRequest {
  const uint8_t* data;
  size_t len;
  const char* name;
};

static QueueHandle_t playQueue = nullptr;
static bool ready = false;

static void setPa(bool enable) {
  const bool level = AUDIO_PA_ACTIVE_HIGH ? enable : !enable;
  digitalWrite(AUDIO_PA_ENABLE_PIN, level ? HIGH : LOW);
}

static void playTask(void* /*arg*/) {
  PlayRequest req;
  while (true) {
    if (xQueueReceive(playQueue, &req, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (!req.data || req.len == 0) {
      continue;
    }

    ESP_LOGI(LOGTAG, "Playing %s (%u bytes, ~%u ms PCM)", req.name ? req.name : "?",
             (unsigned)req.len,
             (unsigned)(req.len * 1000ull / (AUDIO_SAMPLE_RATE * 2 * 2)));
    setPa(true);

    size_t offset = 0;
    while (offset < req.len) {
      size_t chunk = req.len - offset;
      if (chunk > kChunkBytes) {
        chunk = kChunkBytes;
      }
      size_t written = 0;
      esp_err_t err = i2s_write(static_cast<i2s_port_t>(AUDIO_I2S_PORT),
                               req.data + offset, chunk, &written, pdMS_TO_TICKS(1000));
      if (err != ESP_OK) {
        ESP_LOGE(LOGTAG, "i2s_write failed: %s", esp_err_to_name(err));
        break;
      }
      if (written == 0) {
        ESP_LOGW(LOGTAG, "i2s_write wrote 0 bytes");
        break;
      }
      offset += written;
    }
    // Let DMA finish shifting out the last buffers (~8 * 256 frames stereo)
    vTaskDelay(pdMS_TO_TICKS(80));
    ESP_LOGI(LOGTAG, "Play done (%u/%u bytes)", (unsigned)offset, (unsigned)req.len);
  }
}

static bool initI2sTx() {
  i2s_config_t i2s_config = {};
  i2s_config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = AUDIO_SAMPLE_RATE;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = 8;
  i2s_config.dma_buf_len = 256;
  // APLL for accurate 16 kHz * 384 = 6.144 MHz MCLK expected by ES8311
  i2s_config.use_apll = true;
  i2s_config.tx_desc_auto_clear = true;
  i2s_config.fixed_mclk = AUDIO_SAMPLE_RATE * AUDIO_MCLK_MULTIPLE;
  i2s_config.mclk_multiple = I2S_MCLK_MULTIPLE_384;
  i2s_config.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;

  i2s_pin_config_t pin_config = {
      .mck_io_num = AUDIO_I2S_MCLK_PIN,
      .bck_io_num = AUDIO_I2S_BCLK_PIN,
      .ws_io_num = AUDIO_I2S_WS_PIN,
      .data_out_num = AUDIO_I2S_DOUT_PIN,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  esp_err_t err = i2s_driver_install(static_cast<i2s_port_t>(AUDIO_I2S_PORT), &i2s_config, 0, nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(LOGTAG, "i2s_driver_install failed: %s", esp_err_to_name(err));
    return false;
  }
  err = i2s_set_pin(static_cast<i2s_port_t>(AUDIO_I2S_PORT), &pin_config);
  if (err != ESP_OK) {
    ESP_LOGE(LOGTAG, "i2s_set_pin failed: %s", esp_err_to_name(err));
    i2s_driver_uninstall(static_cast<i2s_port_t>(AUDIO_I2S_PORT));
    return false;
  }
  ESP_LOGI(LOGTAG, "I2S TX ready port=%d mclk=%d bclk=%d ws=%d dout=%d",
           AUDIO_I2S_PORT, AUDIO_I2S_MCLK_PIN, AUDIO_I2S_BCLK_PIN, AUDIO_I2S_WS_PIN,
           AUDIO_I2S_DOUT_PIN);
  return true;
}

static void enqueue(const uint8_t* data, size_t len, const char* name) {
  if (!ready || !playQueue || !data || len == 0) {
    ESP_LOGW(LOGTAG, "enqueue skipped ready=%d len=%u name=%s", ready ? 1 : 0, (unsigned)len,
             name ? name : "?");
    return;
  }
  PlayRequest req = {data, len, name};
  xQueueOverwrite(playQueue, &req);
}

bool begin() {
  pinMode(AUDIO_PA_ENABLE_PIN, OUTPUT);
  setPa(true);  // enable amp early so a boot beep / first play is audible
  ESP_LOGI(LOGTAG, "PA enable pin=%d active_high=%d level=%d", AUDIO_PA_ENABLE_PIN,
           AUDIO_PA_ACTIVE_HIGH ? 1 : 0, digitalRead(AUDIO_PA_ENABLE_PIN));

  if (!initI2sTx()) {
    return false;
  }

  if (es8311_codec_init() != ESP_OK) {
    ESP_LOGE(LOGTAG, "ES8311 init failed");
    i2s_driver_uninstall(static_cast<i2s_port_t>(AUDIO_I2S_PORT));
    return false;
  }

  playQueue = xQueueCreate(1, sizeof(PlayRequest));
  if (!playQueue) {
    ESP_LOGE(LOGTAG, "play queue alloc failed");
    return false;
  }

  BaseType_t ok = xTaskCreatePinnedToCore(playTask, "audio_play", 4096, nullptr, 2, nullptr, 0);
  if (ok != pdPASS) {
    ESP_LOGE(LOGTAG, "play task create failed");
    return false;
  }

  ready = true;
  const unsigned pcm_ms =
      (unsigned)(assets_audio_login_raw_len * 1000ull / (AUDIO_SAMPLE_RATE * 2 * 2));
  ESP_LOGI(LOGTAG, "Audio ready — boot beep is %u ms of PCM (%u bytes)", pcm_ms,
           (unsigned)assets_audio_login_raw_len);
  enqueue(assets_audio_login_raw, assets_audio_login_raw_len, "boot/login");
  return true;
}

void playLogin() {
  enqueue(assets_audio_login_raw, assets_audio_login_raw_len, "login");
}

void playToken() {
  enqueue(assets_audio_token_raw, assets_audio_token_raw_len, "token");
}

}  // namespace Audio
