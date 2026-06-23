#pragma once
#include "../hal/IDisplay.h"
#include "../hal/IInput.h"

class SystemUI {
public:
  SystemUI(IDisplay &display);
  ~SystemUI();

  void showError(const char *title, const char *message);
  void showLoading(const char *title, float progress);
  void showSleepCover();
  bool showConfirmDialog(IInput &input, const char *title, const char *message);

private:
  IDisplay &_display;
  bool _fontsLoaded = false;

  void ensureFonts();
};
