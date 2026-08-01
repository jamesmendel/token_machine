#include "audio.h"

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <esp_log.h>
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
static i2s_chan_handle_t txHandle = nullptr;
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
      esp_err_t err = i2s_channel_write(txHandle, req.data + offset, chunk, &written, pdMS_TO_TICKS(1000));
      if (err != ESP_OK) {
        ESP_LOGE(LOGTAG, "i2s_channel_write failed: %s", esp_err_to_name(err));
        break;
      }
      if (written == 0) {
        ESP_LOGW(LOGTAG, "i2s_channel_write wrote 0 bytes");
        break;
      }
      offset += written;
    }
    // Let DMA finish shifting out the last buffers
    vTaskDelay(pdMS_TO_TICKS(80));
    setPa(false);
    ESP_LOGI(LOGTAG, "Play done (%u/%u bytes)", (unsigned)offset, (unsigned)req.len);
  }
}

static bool initI2sTx() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)AUDIO_I2S_PORT, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  esp_err_t err = i2s_new_channel(&chan_cfg, &txHandle, nullptr);  // TX only
  if (err != ESP_OK) {
    ESP_LOGE(LOGTAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
    return false;
  }

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = (gpio_num_t)AUDIO_I2S_MCLK_PIN,
      .bclk = (gpio_num_t)AUDIO_I2S_BCLK_PIN,
      .ws   = (gpio_num_t)AUDIO_I2S_WS_PIN,
      .dout = (gpio_num_t)AUDIO_I2S_DOUT_PIN,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };
  std_cfg.clk_cfg.mclk_multiple = (i2s_mclk_multiple_t)AUDIO_MCLK_MULTIPLE;

  err = i2s_channel_init_std_mode(txHandle, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(LOGTAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
    i2s_del_channel(txHandle);
    txHandle = nullptr;
    return false;
  }

  err = i2s_channel_enable(txHandle);
  if (err != ESP_OK) {
    ESP_LOGE(LOGTAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
    i2s_del_channel(txHandle);
    txHandle = nullptr;
    return false;
  }

  ESP_LOGI(LOGTAG, "I2S TX ready mclk=%d bclk=%d ws=%d dout=%d mclk_mult=%lu",
           AUDIO_I2S_MCLK_PIN, AUDIO_I2S_BCLK_PIN, AUDIO_I2S_WS_PIN, AUDIO_I2S_DOUT_PIN,
           (unsigned long)AUDIO_MCLK_MULTIPLE);
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
  setPa(false);

  if (!initI2sTx()) {
    return false;
  }

  // ES8311 codec init (uses I2C bus already configured by RFID)
  if (es8311_codec_init() != ESP_OK) {
    ESP_LOGE(LOGTAG, "ES8311 init failed");
    i2s_channel_disable(txHandle);
    i2s_del_channel(txHandle);
    txHandle = nullptr;
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
  ESP_LOGI(LOGTAG, "Audio ready — boot sound is %u ms of PCM (%u bytes)", pcm_ms,
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
