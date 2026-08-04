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

  // Height and width are swapped on X4 (landscape)
  // TODO: verify on X3.
  int getWidth() const override { return _eink.getDisplayHeight(); }
  int getHeight() const override { return _eink.getDisplayWidth(); }

  GfxRenderer *getRenderer() override { return &_gfxRenderer; }

private:
  EInkDisplay _eink;
  GfxRenderer _gfxRenderer;
};

#endif // PLATFORM_ESP32
