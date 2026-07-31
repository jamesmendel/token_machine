#include "tag_db.h"

#include <Preferences.h>

namespace TagDb {

static Preferences prefs;
static std::vector<TagRecord> cache;
static bool ready = false;

static String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c >= 32) {
      out += c;
    }
  }
  return out;
}

static String serialize() {
  String json = "[";
  for (size_t i = 0; i < cache.size(); i++) {
    if (i > 0) json += ',';
    json += "{\"uid\":\"";
    json += jsonEscape(cache[i].uid);
    json += "\",\"name\":\"";
    json += jsonEscape(cache[i].name);
    json += "\",\"tokens\":";
    json += String(cache[i].tokens);
    json += '}';
  }
  json += ']';
  return json;
}

static int findIndex(const String& uid) {
  for (size_t i = 0; i < cache.size(); i++) {
    if (cache[i].uid.equalsIgnoreCase(uid)) return (int)i;
  }
  return -1;
}

static bool persist() {
  String json = serialize();
  if (json.length() >= TAG_DB_MAX_JSON) {
    ESP_LOGE("TAGDB", "DB too large (%u bytes)", (unsigned)json.length());
    return false;
  }
  size_t written = prefs.putString(TAG_DB_KEY, json);
  return written > 0;
}

// Minimal parser for [{"uid":"...","name":"...","tokens":N},...]
static bool parse(const String& json) {
  cache.clear();
  int i = 0;
  const int n = (int)json.length();

  auto skipWs = [&]() {
    while (i < n && (json[i] == ' ' || json[i] == '\n' || json[i] == '\r' || json[i] == '\t')) i++;
  };

  auto match = [&](char c) -> bool {
    skipWs();
    if (i < n && json[i] == c) {
      i++;
      return true;
    }
    return false;
  };

  auto parseString = [&](String& out) -> bool {
    skipWs();
    if (i >= n || json[i] != '"') return false;
    i++;
    out = "";
    while (i < n && json[i] != '"') {
      if (json[i] == '\\' && i + 1 < n) {
        i++;
        out += json[i++];
      } else {
        out += json[i++];
      }
    }
    if (i >= n || json[i] != '"') return false;
    i++;
    return true;
  };

  auto parseUint = [&](uint32_t& out) -> bool {
    skipWs();
    if (i >= n || !isDigit(json[i])) return false;
    uint32_t v = 0;
    while (i < n && isDigit(json[i])) {
      v = v * 10 + (uint32_t)(json[i] - '0');
      i++;
    }
    out = v;
    return true;
  };

  skipWs();
  if (!match('[')) {
    if (json.length() == 0) return true;
    return false;
  }

  skipWs();
  if (match(']')) return true;

  while (true) {
    if (!match('{')) return false;

    TagRecord rec;
    bool gotUid = false, gotName = false, gotTokens = false;

    while (true) {
      String key;
      if (!parseString(key)) return false;
      if (!match(':')) return false;

      if (key == "uid") {
        if (!parseString(rec.uid)) return false;
        gotUid = true;
      } else if (key == "name") {
        if (!parseString(rec.name)) return false;
        gotName = true;
      } else if (key == "tokens") {
        if (!parseUint(rec.tokens)) return false;
        gotTokens = true;
      } else {
        // skip unknown value (string or number)
        skipWs();
        if (i < n && json[i] == '"') {
          String tmp;
          if (!parseString(tmp)) return false;
        } else if (i < n && isDigit(json[i])) {
          uint32_t tmp;
          if (!parseUint(tmp)) return false;
        } else {
          return false;
        }
      }

      skipWs();
      if (match(',')) continue;
      if (match('}')) break;
      return false;
    }

    if (gotUid && gotName && gotTokens) {
      if (rec.name.length() > TAG_NAME_MAX) rec.name = rec.name.substring(0, TAG_NAME_MAX);
      cache.push_back(rec);
    }

    skipWs();
    if (match(',')) continue;
    if (match(']')) return true;
    return false;
  }
}

bool begin() {
  if (!prefs.begin(TAG_DB_NAMESPACE, false)) {
    ESP_LOGE("TAGDB", "Preferences begin failed");
    return false;
  }
  String json = prefs.getString(TAG_DB_KEY, "[]");
  if (!parse(json)) {
    ESP_LOGW("TAGDB", "Corrupt DB, resetting");
    cache.clear();
    persist();
  }
  ready = true;
  ESP_LOGI("TAGDB", "Loaded %u tags", (unsigned)cache.size());
  return true;
}

bool get(const String& uid, String& name, uint32_t& tokens) {
  int idx = findIndex(uid);
  if (idx < 0) return false;
  name = cache[idx].name;
  tokens = cache[idx].tokens;
  return true;
}

bool exists(const String& uid) {
  return findIndex(uid) >= 0;
}

bool upsert(const String& uid, const String& name, uint32_t tokens) {
  if (!ready || uid.length() == 0 || uid.length() > TAG_UID_MAX) return false;
  String clipped = name;
  if (clipped.length() > TAG_NAME_MAX) clipped = clipped.substring(0, TAG_NAME_MAX);

  int idx = findIndex(uid);
  if (idx >= 0) {
    cache[idx].name = clipped;
    cache[idx].tokens = tokens;
  } else {
    TagRecord rec;
    rec.uid = uid;
    rec.uid.toLowerCase();
    rec.name = clipped;
    rec.tokens = tokens;
    cache.push_back(rec);
  }
  return persist();
}

bool setTokens(const String& uid, uint32_t tokens) {
  int idx = findIndex(uid);
  if (idx < 0) return false;
  cache[idx].tokens = tokens;
  return persist();
}

bool remove(const String& uid) {
  int idx = findIndex(uid);
  if (idx < 0) return false;
  cache.erase(cache.begin() + idx);
  return persist();
}

std::vector<TagRecord> listAll() {
  return cache;
}

String toJsonArray() {
  return serialize();
}

}  // namespace TagDb
