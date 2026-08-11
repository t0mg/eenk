#pragma once
#include "../hal/IDisplay.h"

class LoadingWidget {
public:
  static void show(IDisplay &display, const char *title, float progress);
  static void show(IDisplay &display, int barX, int barY, int barWidth,
                   int barHeight, const char *title, float progress,
                   bool shadowedBox = true);
  static void showMessage(IDisplay &display, const char *title,
                          const char *message, bool halftoneBackground = false);
};
