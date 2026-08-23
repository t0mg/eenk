#pragma once
#include "../hal/IDisplay.h"
#include "../hal/IInput.h"
#include <string>
#include <vector>

class SystemUI {
public:
  SystemUI(IDisplay &display);
  ~SystemUI();

  void showMessage(const char *title, const char *message);
  void showLoading(const char *title, float progress);
  // Displays a full-screen sleeping cover.
  void showSleepCover(const char *msg = "Sleeping...", const char *title = nullptr);
  bool showConfirmDialog(IInput &input, const char *title, const char *message,
                         const char *headerTitle = "", int minWidth = 0,
                         int minHeight = 0, bool drawHalftone = true);
  int showMenuModal(IInput &input, const char *title,
                    const std::vector<std::string> &items,
                    int initialSelection = 0, const char *headerTitle = "",
                    int minWidth = 0, int minHeight = 0,
                    bool drawHalftone = true,
                    int *outW = nullptr, int *outH = nullptr);

  // Checks battery level and forces sleep if < 5%. Safe to call frequently.
  static void checkBatteryAndShutdown(class BatteryWidget& battery, class IDisplay& display);

private:
  IDisplay &_display;
};
