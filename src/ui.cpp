#include "ui.h"

#include <TFT_eSPI.h>
#include "config.h"
#include "image_data/logo_broomfield_stem.h"
#include "image_data/logo_broomfield_robotics.h"

namespace Ui {

static TFT_eSPI lcd;
static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_FG = TFT_WHITE;
static const uint16_t COL_ACCENT = TFT_CYAN;
static const uint16_t COL_MUTED = TFT_LIGHTGREY;
static const uint16_t COL_WARN = TFT_ORANGE;
static const uint16_t COL_PRESENT = TFT_GREEN;
static const uint16_t COL_ABSENT = TFT_DARKGREY;

static bool onDashboard = false;
static bool tagPresentDrawn = false;

static const int16_t PROX_R = 8;
static const int16_t PROX_MARGIN = 14;

static void drawProximityIndicator(bool present) {
  int16_t x = lcd.width() - PROX_MARGIN;
  int16_t y = PROX_MARGIN;
  // Clear a small area so outline/fill swaps cleanly
  lcd.fillCircle(x, y, PROX_R + 2, COL_BG);
  if (present) {
    lcd.fillCircle(x, y, PROX_R, COL_PRESENT);
  } else {
    lcd.drawCircle(x, y, PROX_R, COL_ABSENT);
  }
  tagPresentDrawn = present;
}

static void drawCentered(const String& text, int16_t y, uint8_t font, uint16_t color) {
  lcd.setTextDatum(MC_DATUM);
  lcd.setTextColor(color, COL_BG);
  lcd.drawString(text, lcd.width() / 2, y, font);
}

void begin() {
  lcd.init();
  lcd.setRotation(UI_ROTATION);
  lcd.fillScreen(COL_BG);
  onDashboard = false;
  showIdle();
}

void showIdle() {
  onDashboard = false;
  lcd.fillScreen(COL_BG);

  // Render Broomfield STEM logo — 20px padding each side, 8bpp indexed
  int16_t logoX = (lcd.width()  - logo_broomfield_robotics_width)  / 2;
  int16_t logoY = 40;
  lcd.pushImage(logoX, logoY, logo_broomfield_robotics_width, logo_broomfield_robotics_height,
                logo_broomfield_robotics_data, true);

  // int16_t textY = logoY + logo_broomfield_stem_height + 50;
  drawCentered("Tap card to login", 170, 4, COL_FG);
}

void showDashboard(const String& name, uint32_t tokens, bool tagPresent, int logoutSeconds) {
  onDashboard = true;
  lcd.fillScreen(COL_BG);

  drawCentered(name, 50, 4, COL_ACCENT);
  drawCentered("High Fives", 100, 2, COL_MUTED);
  drawCentered(String(tokens), 140, 6, COL_FG);
  drawCentered("Tap card again to add High Fives", 195, 2, COL_MUTED);

  if (logoutSeconds >= 0) {
    String msg = "Logging out in ";
    msg += String(logoutSeconds);
    msg += " seconds.";
    drawCentered(msg, 225, 2, COL_WARN);
  }

  drawProximityIndicator(tagPresent);
}

void setTagPresent(bool present) {
  if (!onDashboard || present == tagPresentDrawn) return;
  drawProximityIndicator(present);
}

void showUnknown(const String& uid) {
  onDashboard = false;
  lcd.fillScreen(COL_BG);
  drawCentered("Unknown card", lcd.height() / 2 - 20, 4, COL_WARN);
  if (uid.length() > 0) {
    drawCentered(uid, lcd.height() / 2 + 20, 2, COL_MUTED);
  }
}

}  // namespace Ui
