#pragma once
#include "BatteryWidget.h"
#include "hal/IDisplay.h"
#include "hal/IFrontlight.h"
#include "hal/IInput.h"
#include "os/AppSettings.h"

enum class QuickMenuAction { NONE, CLOSE, OPEN_SETTINGS, SLEEP_DEVICE };

class QuickMenuWidget {
public:
  QuickMenuWidget(IDisplay &display, IInput &input, BatteryWidget &battery,
                  IFrontlight *frontlight, AppSettings &settings,
                  bool choicesEnabled = true);
  ~QuickMenuWidget() = default;

  QuickMenuAction show();

private:
  IDisplay &_display;
  IInput &_input;
  BatteryWidget &_battery;
  IFrontlight *_frontlight;
  AppSettings &_settings;
  bool _choicesEnabled = true;
  bool _dirty = false;

  int _selectedRow =
      0; // 0: Cool Brightness, 1: Warmth, 2: Sleep, 3: Settings, 4: Close

  void render();
};
