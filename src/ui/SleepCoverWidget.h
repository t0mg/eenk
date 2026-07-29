#pragma once
#include "../hal/IDisplay.h"

class SleepCoverWidget {
public:
  static void show(IDisplay &display, const char *msg = "Sleeping...", const char *title = nullptr);
};
