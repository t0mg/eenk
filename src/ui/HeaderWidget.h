#pragma once

#include "BatteryWidget.h"
#include "hal/IDisplay.h"

class HeaderWidget {
public:
  static constexpr int HEIGHT = 40;
  static constexpr int BEZEL_OFFSET_Y =
      4; // Offset to push elements down to avoid top bezel
  static constexpr int LEFT_MARGIN = 8;

  HeaderWidget(IDisplay &display, BatteryWidget &battery);

  // Renders the header with the given title string.
  void render(const char *title, int fontIndex) const;

private:
  IDisplay &_display;
  BatteryWidget &_battery;
};
