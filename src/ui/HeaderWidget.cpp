#include "HeaderWidget.h"
#include "NeuStyle.h"
#include <GfxRenderer.h>

HeaderWidget::HeaderWidget(IDisplay &display, BatteryWidget &battery)
    : _display(display), _battery(&battery) {}

HeaderWidget::HeaderWidget(IDisplay &display, BatteryWidget *battery)
    : _display(display), _battery(battery) {}

void HeaderWidget::render(const char *title, int fontIndex) const {
  auto *r = _display.getRenderer();
  if (!r)
    return;

#if defined(PLATFORM_ESP32) || defined(PLATFORM_NATIVE) ||                     \
    defined(PIO_UNIT_TESTING)
  // Poll battery so the widget shows a fresh reading on first render.
  if (_battery) {
    _battery->tick();
  }

  int displayW = _display.getWidth();

  // Neubrutalist header: solid black background.
  r->fillRect(0, 0, displayW, HEIGHT, true /*black*/);

  // Right: battery widget (inverted = white on black).
  if (_battery) {
    int batX = displayW - BatteryWidget::getWidth() - LEFT_MARGIN;
    int batY = BEZEL_OFFSET_Y +
               (HEIGHT - BEZEL_OFFSET_Y - BatteryWidget::getHeight()) / 2;
    _battery->draw(batX, batY, true /*inverted — white on black*/);
  }

  // Left: Title in heading font, white on black, vertically centred.
  int fontH = r->getLineHeight(fontIndex);
  int textY = BEZEL_OFFSET_Y + (HEIGHT - BEZEL_OFFSET_Y - fontH) / 2;
  // Battery widget icon (44px) + label (up to ~110px) + spacing: allocate 180px
  // on the right when battery widget is present
  int maxTitleW = _battery ? (displayW - 180) : (displayW - LEFT_MARGIN * 2);

  char upper[128];
  int i = 0;
  if (title && title[0] != '\0') {
    // Convert provided title to uppercase for neubrutalist style.
    for (; title[i] && i < 120; i++) {
      upper[i] = toupper((unsigned char)title[i]);
    }
    upper[i] = '\0';
  } else {
    // Default logo title when no title is provided (lowercase "eenk").
    strcpy(upper, "eenk");
    i = 4;
  }

  if (r->getTextWidth(fontIndex, upper) > maxTitleW) {
    char *sep = strstr(upper, " - ");
    if (sep && sep > upper + 3) {
      // Preserve the suffix " - ...", shorten prefix before the separator
      std::string suffixStr(sep);
      int prefixLen = sep - upper;
      while (prefixLen > 3 && r->getTextWidth(fontIndex, upper) > maxTitleW) {
        prefixLen--;
        upper[prefixLen - 3] = '.';
        upper[prefixLen - 2] = '.';
        upper[prefixLen - 1] = '.';
        strcpy(upper + prefixLen, suffixStr.c_str());
      }
    }
    // Fallback standard right-truncation if no separator or still too wide
    int curLen = strlen(upper);
    while (curLen > 3 && r->getTextWidth(fontIndex, upper) > maxTitleW) {
      curLen--;
      upper[curLen - 3] = '.';
      upper[curLen - 2] = '.';
      upper[curLen - 1] = '.';
      upper[curLen] = '\0';
    }
  }

  r->drawText(fontIndex, LEFT_MARGIN, textY, upper, false /*white*/);

#else
  (void)title;
  (void)fontIndex;
#endif
}
