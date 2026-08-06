#pragma once
#ifdef PLATFORM_ESP32

#include "hal/IDisplay.h"
#include <EInkDisplay.h>
#include <GfxRenderer.h>

class EspEinkDisplay : public IDisplay {
public:
  EspEinkDisplay();
  ~EspEinkDisplay() override = default;

  void clear() override;
  void present() override;
  void fullRefresh() override;

  int getWidth() const override { return _gfxRenderer.getScreenWidth(); }
  int getHeight() const override { return _gfxRenderer.getScreenHeight(); }

  GfxRenderer *getRenderer() override { return &_gfxRenderer; }

private:
  EInkDisplay _eink;
  GfxRenderer _gfxRenderer;
};

#endif // PLATFORM_ESP32
