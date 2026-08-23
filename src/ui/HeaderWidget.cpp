#include "HeaderWidget.h"
#include "NeuStyle.h"
#include <GfxRenderer.h>

HeaderWidget::HeaderWidget(IDisplay &display, BatteryWidget &battery)
    : _display(display), _battery(battery) {}

void HeaderWidget::render(const char *title, int fontIndex) const {
  auto *r = _display.getRenderer();
  if (!r)
    return;

#if defined(PLATFORM_ESP32) || defined(PLATFORM_NATIVE) || defined(PIO_UNIT_TESTING)
  // Poll battery so the widget shows a fresh reading on first render.
  _battery.tick();

  int displayW = _display.getWidth();

  // Neubrutalist header: solid black background.
  r->fillRect(0, 0, displayW, HEIGHT, true /*black*/);

  // Right: battery widget (inverted = white on black).
  int batX = displayW - BatteryWidget::getWidth() - LEFT_MARGIN;
  int batY = BEZEL_OFFSET_Y +
             (HEIGHT - BEZEL_OFFSET_Y - BatteryWidget::getHeight()) / 2;
  _battery.draw(batX, batY, true /*inverted — white on black*/);

  // Left: Title in heading font, white on black, vertically centred.
  int fontH = r->getLineHeight(fontIndex);
  int textY = BEZEL_OFFSET_Y + (HEIGHT - BEZEL_OFFSET_Y - fontH) / 2;
  // Battery widget icon (44px) + label (up to ~110px) + spacing: allocate 180px on the right
  int maxTitleW = displayW - 180;

  // Convert title to uppercase for neubrutalist style.
  if (title && title[0] != '\0') {
    char upper[64];
    int i = 0;
    for (; title[i] && i < 60; i++) {
      upper[i] = toupper((unsigned char)title[i]);
    }
    upper[i] = '\0';

    if (r->getTextWidth(fontIndex, upper) > maxTitleW) {
      // Shorten string and append ellipsis
      while (i > 3 && r->getTextWidth(fontIndex, upper) > maxTitleW) {
        i--;
        upper[i - 3] = '.';
        upper[i - 2] = '.';
        upper[i - 1] = '.';
        upper[i] = '\0';
      }
    }

    r->drawText(fontIndex, LEFT_MARGIN, textY, upper, false /*white*/);
  }

#else
  (void)title;
  (void)fontIndex;
#endif
}
