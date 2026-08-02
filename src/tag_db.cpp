#include "tag_db.h"

#include <Preferences.h>
#include <ArduinoJson.h>

namespace TagDb {

static Preferences prefs;
static std::vector<TagRecord> cache;
static bool ready = false;

static String serialize() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  for (const auto& rec : cache) {
    JsonObject obj = arr.add<JsonObject>();
    obj["uid"] = rec.uid;
    obj["name"] = rec.name;
    obj["tokens"] = rec.tokens;
  }

  String json;
  serializeJson(doc, json);
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

static bool parse(const String& json) {
  cache.clear();
  if (json.length() == 0) return true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    ESP_LOGW("TAGDB", "Parse error: %s", err.c_str());
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull()) return false;

  for (JsonObject obj : arr) {
    const char* uid = obj["uid"];
    const char* name = obj["name"];
    uint32_t tokens = obj["tokens"] | 0UL;

    if (uid && name) {
      TagRecord rec;
      rec.uid = String(uid);
      rec.name = String(name);
      if (rec.name.length() > TAG_NAME_MAX) {
        rec.name = rec.name.substring(0, TAG_NAME_MAX);
      }
      rec.tokens = tokens;
      cache.push_back(rec);
    }
  }

  return true;
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
