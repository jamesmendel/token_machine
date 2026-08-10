#pragma once

#include <Arduino.h>

namespace Ui {

void begin();
void showIdle();
void showDashboard(const String& name, uint32_t totalTokens, uint32_t sessionAdded, bool tagPresent, int logoutSeconds = -1);
void setTagPresent(bool present);
void showUnknown(const String& uid);

}  // namespace Ui
