#include "ui.h"

#include <TFT_eSPI.h>
#include "config.h"

namespace Ui {

static TFT_eSPI lcd;
static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_FG = TFT_WHITE;
static const uint16_t COL_ACCENT = TFT_CYAN;
static const uint16_t COL_MUTED = TFT_LIGHTGREY;
static const uint16_t COL_WARN = TFT_ORANGE;

void begin() {
  lcd.init();
  lcd.setRotation(UI_ROTATION);
  lcd.fillScreen(COL_BG);
  showIdle();
}

static void drawCentered(const String& text, int16_t y, uint8_t font, uint16_t color) {
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextColor(color, COL_BG);
  lcd.drawString(text, lcd.width() / 2, y, font);
}

void showIdle() {
  lcd.fillScreen(COL_BG);
  drawCentered("Waiting for tag...", lcd.height() / 2, 4, COL_FG);
}

void showDashboard(const String& name, uint32_t tokens, int logoutSeconds) {
  lcd.fillScreen(COL_BG);

  drawCentered(name, 50, 4, COL_ACCENT);

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextColor(COL_MUTED, COL_BG);
  lcd.drawString("Tokens", lcd.width() / 2, 100, 2);

  drawCentered(String(tokens), 140, 6, COL_FG);

  drawCentered("Present tag to add tokens", 195, 2, COL_MUTED);

  if (logoutSeconds >= 0) {
    String msg = "Logging out in ";
    msg += String(logoutSeconds);
    msg += " seconds.";
    drawCentered(msg, 225, 2, COL_WARN);
  }
}

void showUnknown(const String& uid) {
  lcd.fillScreen(COL_BG);
  drawCentered("Unknown tag", lcd.height() / 2 - 20, 4, COL_WARN);
  if (uid.length() > 0) {
    drawCentered(uid, lcd.height() / 2 + 20, 2, COL_MUTED);
  }
}

}  // namespace Ui
