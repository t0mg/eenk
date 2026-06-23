#include "HeaderWidget.h"
#include <GfxRenderer.h>

HeaderWidget::HeaderWidget(IDisplay& display, BatteryWidget& battery)
    : _display(display), _battery(battery) {}

void HeaderWidget::render(const char* title, int fontIndex) const {
    auto* r = _display.getRenderer();
    if (!r) return;

#if defined(PLATFORM_ESP32) || defined(PIO_UNIT_TESTING)
    // Poll battery so the widget shows a fresh reading on first render.
    _battery.tick();

    int displayW = _display.getWidth();

    // Clear status bar strip.
    r->fillRect(0, 0, displayW, HEIGHT, false /*white*/);

    // Left: Title in bold, vertically centred within the bezel-adjusted area.
    int fontH = r->getLineHeight(fontIndex);
    // Centered in the area between BEZEL_OFFSET_Y and HEIGHT
    int textY = BEZEL_OFFSET_Y + (HEIGHT - BEZEL_OFFSET_Y - fontH) / 2;
    r->drawText(fontIndex, LEFT_MARGIN, textY, title, true /*black*/);

    // Right: battery widget.
    // Label is drawn LEFT of the body, so icon body+nub (27px) sits in the
    // top-right corner. The label extends leftward automatically.
    static constexpr int kIconW = 27; // body(24) + nub(3)
    static constexpr int kBatH = 14;
    int batX = displayW - kIconW - LEFT_MARGIN;
    int batY = BEZEL_OFFSET_Y + (HEIGHT - BEZEL_OFFSET_Y - kBatH) / 2;
    _battery.draw(batX, batY, false /*not inverted*/);

    // Bottom separator line.
    r->drawLine(0, HEIGHT - 1, displayW, HEIGHT - 1, true);
#else
    (void)title;
    (void)fontIndex;
#endif
}
