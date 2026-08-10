#include "ui.h"

#include <TFT_eSPI.h>
#include "config.h"
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

static const int16_t PROX_R = 12;
static const int16_t PROX_MARGIN = 18;

static void drawProximityIndicator(bool present) {
  int16_t x = PROX_MARGIN;
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

static void drawCentered(const String& text, int16_t y, uint8_t fsize, uint16_t color, bool bold = false) {
  if(!bold) {
    switch(fsize) {
      case 1: lcd.setFreeFont(&FreeSans9pt7b); break;
      case 2: lcd.setFreeFont(&FreeSans12pt7b); break;
      case 3: lcd.setFreeFont(&FreeSans18pt7b); break;
      case 4: lcd.setFreeFont(&FreeSans24pt7b); break;
      default: lcd.setFreeFont(&FreeSans12pt7b); break;
    }
  }
  else {
      switch(fsize) {
        case 1: lcd.setFreeFont(&FreeSansBold9pt7b); break;
        case 2: lcd.setFreeFont(&FreeSansBold12pt7b); break;
        case 3: lcd.setFreeFont(&FreeSansBold18pt7b); break;
        case 4: lcd.setFreeFont(&FreeSansBold24pt7b); break;
        default: lcd.setFreeFont(&FreeSansBold12pt7b); break;
      }
    }

  lcd.setTextDatum(MC_DATUM);
  lcd.setTextColor(color, COL_BG);
  lcd.drawString(text, lcd.width() / 2, y);
  // lcd.drawString(text, lcd.width() / 2, y, font);
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
  drawCentered("Tap card to login", 170, 3, COL_FG);
}

void showDashboard(const String& name, uint32_t totalTokens, uint32_t sessionAdded, bool tagPresent, int logoutSeconds) {
  onDashboard = true;
  lcd.fillScreen(COL_BG);

  drawCentered(name, 35, 3, COL_ACCENT);

  // Session added tokens
  String addedStr = "+" + String(sessionAdded);
  drawCentered(addedStr, 95, 3, COL_FG, true);
  drawCentered("High Fives added", 130, 1, COL_FG);

  // Total token count
  String totalStr = "Your total: " + String(totalTokens);
  drawCentered(totalStr, 175, 1, COL_MUTED);

  drawCentered("Tap card again to add High Fives", 200, 1, COL_MUTED);

  if (logoutSeconds >= 0) {
    String msg = "Logging out in ";
    msg += String(logoutSeconds);
    msg += " seconds.";
    drawCentered(msg, 225, 1, COL_WARN);
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
  drawCentered("Unknown card", lcd.height() / 2 - 20, 3, COL_WARN);
  if (uid.length() > 0) {
    drawCentered(uid, lcd.height() / 2 + 20, 1, COL_MUTED);
  }
}

}  // namespace Ui
