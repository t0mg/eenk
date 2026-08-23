#pragma once
#include "../hal/IDisplay.h"
#include "../hal/IInput.h"

class SystemUI {
public:
  SystemUI(IDisplay &display);
  ~SystemUI();

  void showMessage(const char *title, const char *message);
  void showLoading(const char *title, float progress);
  // Displays a full-screen sleeping cover.
  void showSleepCover(const char *msg = "Sleeping...", const char *title = nullptr);
  bool showConfirmDialog(IInput &input, const char *title, const char *message, const char *headerTitle = "");

  // Checks battery level and forces sleep if < 5%. Safe to call frequently.
  static void checkBatteryAndShutdown(class BatteryWidget& battery, class IDisplay& display);

private:
  IDisplay &_display;
};
