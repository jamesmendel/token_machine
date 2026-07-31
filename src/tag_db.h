#pragma once

#include <Arduino.h>
#include <vector>
#include "config.h"

struct TagRecord {
  String uid;
  String name;
  uint32_t tokens;
};

namespace TagDb {

bool begin();
bool get(const String& uid, String& name, uint32_t& tokens);
bool upsert(const String& uid, const String& name, uint32_t tokens);
bool setTokens(const String& uid, uint32_t tokens);
bool remove(const String& uid);
bool exists(const String& uid);
std::vector<TagRecord> listAll();
String toJsonArray();

}  // namespace TagDb
