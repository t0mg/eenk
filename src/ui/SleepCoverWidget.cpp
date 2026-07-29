#include "SleepCoverWidget.h"
#include <GfxRenderer.h>
#include <cstdio>

void SleepCoverWidget::show(IDisplay &display, const char *msg, const char *title) {
  auto renderer = display.getRenderer();
  if (renderer) {
    display.clear();

#ifdef PLATFORM_ESP32
    if (title && title[0] != '\0') {
      renderer->drawText(11, 50, 50, title);
    } else {
      renderer->drawText(11, 50, 50, "eenk");
    }
    renderer->drawText(10, 50, 90, msg);
#endif

    display.fullRefresh();
  } else {
    printf("\nDevice sleeping...\n");
  }
}
