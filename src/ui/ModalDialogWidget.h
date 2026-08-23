#pragma once
#include "../hal/IDisplay.h"
#include "../hal/IInput.h"
#include "BatteryWidget.h"

class ModalDialogWidget {
public:
  // Shows a confirmation dialog and blocks until the user dismisses it or
  // confirms. Returns true if confirmed (CONFIRM button), false otherwise (QUIT
  // or CANCEL).
  static bool show(IDisplay &display, IInput &input,
                   BatteryWidget *batteryWidget, const char *title,
                   const char *message, const char *headerTitle = "",
                   int minWidth = 0, int minHeight = 0,
                   bool drawHalftone = true);
};
