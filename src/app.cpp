#include "app.h"

#include "config.h"
#include "tag_db.h"
#include "ui.h"

namespace App {

enum class State {
  Idle,
  Dashboard,
  Countdown,
  Unknown,
};

static State state = State::Idle;
static String sessionUid;
static String sessionName;
static uint32_t sessionTokens = 0;
static bool tagWasRemoved = false;  // true after Disappeared while in session
static bool tagPresent = false;
static uint32_t lastActivityMs = 0;
static int lastCountdownShown = -1;
static uint32_t unknownSinceMs = 0;

static void enterIdle() {
  state = State::Idle;
  sessionUid = "";
  sessionName = "";
  sessionTokens = 0;
  tagWasRemoved = false;
  tagPresent = false;
  lastCountdownShown = -1;
  Ui::showIdle();
  ESP_LOGI("APP", "State: Idle");
}

static void refreshDashboard(int logoutSeconds = -1) {
  Ui::showDashboard(sessionName, sessionTokens, tagPresent, logoutSeconds);
}

static void enterDashboard(const String& uid, const String& name, uint32_t tokens, bool increment) {
  sessionUid = uid;
  sessionName = name;
  sessionTokens = tokens;

  if (increment) {
    sessionTokens++;
    TagDb::setTokens(sessionUid, sessionTokens);
    ESP_LOGI("APP", "Increment %s -> %u", sessionUid.c_str(), (unsigned)sessionTokens);
  }

  tagWasRemoved = false;
  tagPresent = true;
  lastActivityMs = millis();
  lastCountdownShown = -1;
  state = State::Dashboard;
  refreshDashboard();
  ESP_LOGI("APP", "State: Dashboard (%s)", sessionName.c_str());
}

static void enterUnknown(const String& uid) {
  state = State::Unknown;
  sessionUid = uid;
  tagWasRemoved = false;
  unknownSinceMs = millis();
  lastActivityMs = unknownSinceMs;
  Ui::showUnknown(uid);
  ESP_LOGI("APP", "State: Unknown (%s)", uid.c_str());
}

static void noteActivity() {
  lastActivityMs = millis();
  if (state == State::Countdown) {
    state = State::Dashboard;
    lastCountdownShown = -1;
    refreshDashboard();
    ESP_LOGI("APP", "State: Dashboard (activity)");
  }
}

static void setSessionTagPresent(bool present) {
  if (tagPresent == present) return;
  tagPresent = present;
  if (state == State::Dashboard || state == State::Countdown) {
    Ui::setTagPresent(present);
  }
}

void begin() {
  enterIdle();
}

void onRfidEvent(const RfidEvent& event) {
  switch (event.type) {
    case RfidEventType::None:
    case RfidEventType::StillPresent:
      break;

    case RfidEventType::Disappeared:
      if (state == State::Dashboard || state == State::Countdown) {
        if (event.uid.equalsIgnoreCase(sessionUid)) {
          tagWasRemoved = true;
          setSessionTagPresent(false);
          noteActivity();
        }
      } else if (state == State::Unknown) {
        enterIdle();
      }
      break;

    case RfidEventType::Appeared: {
      String name;
      uint32_t tokens = 0;
      bool registered = TagDb::get(event.uid, name, tokens);

      if (state == State::Idle || state == State::Unknown) {
        if (registered) {
          enterDashboard(event.uid, name, tokens, false);
        } else {
          enterUnknown(event.uid);
        }
        break;
      }

      // Dashboard or Countdown
      if (registered) {
        if (event.uid.equalsIgnoreCase(sessionUid)) {
          if (tagWasRemoved) {
            // Re-present after removal → increment
            TagDb::get(event.uid, name, tokens);  // refresh in case web edited
            enterDashboard(event.uid, name, tokens, true);
          } else {
            setSessionTagPresent(true);
            noteActivity();
          }
        } else {
          // Different registered user
          enterDashboard(event.uid, name, tokens, false);
        }
      } else {
        enterUnknown(event.uid);
      }
      break;
    }
  }
}

void tick(uint32_t nowMs) {
  if (state == State::Unknown) {
    if (nowMs - unknownSinceMs >= UNKNOWN_TAG_TIMEOUT_MS) {
      enterIdle();
    }
    return;
  }

  if (state != State::Dashboard && state != State::Countdown) {
    return;
  }

  uint32_t idleMs = nowMs - lastActivityMs;

  if (idleMs >= SESSION_LOGOUT_MS) {
    enterIdle();
    return;
  }

  if (idleMs >= SESSION_COUNTDOWN_START_MS) {
    uint32_t remainingMs = SESSION_LOGOUT_MS - idleMs;
    int secs = (int)((remainingMs + 999) / 1000);  // ceil
    if (secs < 1) secs = 1;

    if (state != State::Countdown || secs != lastCountdownShown) {
      state = State::Countdown;
      lastCountdownShown = secs;
      refreshDashboard(secs);
    }
  }
}

}  // namespace App
