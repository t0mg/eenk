#pragma once

#include "../hal/IDisplay.h"
#include "../hal/IInput.h"
#include "BatteryWidget.h"
#include <string>
#include <vector>

class MenuModalWidget {
public:
  /**
   * Shows a selectable menu modal dialog and blocks until the user confirms a
   * selection or cancels.
   *
   * @param display Display interface
   * @param input Input interface
   * @param batteryWidget Optional battery widget to render in header
   * @param title Modal title (e.g. "STORY MENU", "REWIND TO...")
   * @param items List of item text strings
   * @param initialSelection Initial selected index (0-based)
   * @param headerTitle Header bar text (e.g. "eenk")
   * @return Selected item index (0 to items.size()-1), or -1 if cancelled.
   */
  static int show(IDisplay &display, IInput &input,
                  BatteryWidget *batteryWidget, const char *title,
                  const std::vector<std::string> &items,
                  int initialSelection = 0, const char *headerTitle = "",
                  int minWidth = 0, int minHeight = 0,
                  bool drawHalftone = true,
                  int *outW = nullptr, int *outH = nullptr);
};
