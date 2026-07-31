#pragma once
#include "../hal/IDisplay.h"

class LoadingWidget {
public:
  static void show(IDisplay &display, const char *title, float progress);
  static void showMessage(IDisplay &display, const char *title, const char *message);
};
