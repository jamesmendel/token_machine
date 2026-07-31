#pragma once

#include <Arduino.h>

namespace Ui {

void begin();
void showIdle();
void showDashboard(const String& name, uint32_t tokens, int logoutSeconds = -1);
void showUnknown(const String& uid);

}  // namespace Ui
