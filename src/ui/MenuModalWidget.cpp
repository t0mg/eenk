#include "MenuModalWidget.h"
#include "FooterWidget.h"
#include "HeaderWidget.h"
#include "NeuStyle.h"
#include <GfxRenderer.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#ifdef PLATFORM_ESP32
#include <Arduino.h>
#endif

struct RenderedMenuItem {
  int x, y, w, h;
  int itemIndex;
};

int MenuModalWidget::show(IDisplay &display, IInput &input,
                          BatteryWidget *batteryWidget, const char *title,
                          const std::vector<std::string> &items,
                          int initialSelection, const char *headerTitle,
                          int minWidth, int minHeight,
                          bool drawHalftone,
                          int *outW, int *outH) {
  auto renderer = display.getRenderer();
  if (!renderer || items.empty())
    return -1;

  const int dispW = display.getWidth();
  const int dispH = display.getHeight();

  int DLG_W = std::max(static_cast<int>(NeuStyle::MENU_DIALOG_W), minWidth);
  int fontHeading = NeuStyle::FONT_HEADING;
  int fontBody = NeuStyle::FONT_BODY;

  bool hasTitle = (title && title[0] != '\0');
  int headingLineH = hasTitle ? renderer->getLineHeight(fontHeading) : 0;
  int titleBarH = hasTitle ? (headingLineH + 32) : 0;
  int bodyLineH = renderer->getLineHeight(fontBody);

  int totalItems = static_cast<int>(items.size());
  int selectedIndex = std::max(0, std::min(initialSelection, totalItems - 1));

  static constexpr int maxItemsPerPage = 4;

  while (true) {
    int currentPage = selectedIndex / maxItemsPerPage;
    int startIndex = currentPage * maxItemsPerPage;
    int endIndex = std::min(totalItems, startIndex + maxItemsPerPage);

    int innerX = (dispW - DLG_W) / 2 + NeuStyle::BORDER_W;
    int innerW = DLG_W - 2 * NeuStyle::BORDER_W;
    int maxTextW = innerW - 44;

    // Measure item heights for current page
    std::vector<std::vector<std::string>> pageWrappedLines;
    std::vector<int> pageItemHeights;
    int itemsTotalH = 0;

    for (int i = startIndex; i < endIndex; ++i) {
      std::vector<std::string> wrapped;
      if (!items[i].empty()) {
        wrapped = renderer->wrapTextWithHyphenation(fontBody, items[i].c_str(),
                                                    maxTextW, 20);
      }
      if (wrapped.empty()) {
        wrapped.push_back(items[i]);
      }
      pageWrappedLines.push_back(wrapped);

      int numLines = static_cast<int>(wrapped.size());
      int itemH = std::max(56, numLines * bodyLineH + 20);
      pageItemHeights.push_back(itemH);
      itemsTotalH += itemH;
    }

    int calculatedH =
        (hasTitle ? titleBarH : 0) + itemsTotalH + 2 * NeuStyle::BORDER_W;
    int DLG_H = std::max(calculatedH, minHeight);
    int dlgX = (dispW - DLG_W) / 2;
    int dlgY =
        (dispH - FooterWidget::HEIGHT - NeuStyle::SHADOW_OFFSET - DLG_H) / 2;

    if (hasTitle) {
      dlgY += titleBarH / 2;
    }

    if (outW) *outW = DLG_W;
    if (outH) *outH = DLG_H;

    // Draw halftone background overlay if requested
    if (drawHalftone) {
      renderer->fillHalftoneRect(0, 0, dispW, dispH, false);
    }

    // Shadow box: 6px border, 10px offset shadow
    renderer->drawShadowBox(dlgX, dlgY, DLG_W, DLG_H, NeuStyle::BORDER_W,
                            NeuStyle::SHADOW_OFFSET);

    // Header widget
    if (batteryWidget) {
      HeaderWidget header(display, *batteryWidget);
      header.render(headerTitle ? headerTitle : "", fontHeading);
    }

    // Title bar
    int currY = dlgY + NeuStyle::BORDER_W;
    if (hasTitle) {
      renderer->fillRect(dlgX, currY, DLG_W, titleBarH, true);
      std::string upperTitle(title);
      for (auto &c : upperTitle)
        c = toupper(c);
      int tw = renderer->getTextWidth(fontHeading, upperTitle.c_str());
      int titleTextY = currY + (titleBarH - headingLineH) / 2;
      renderer->drawText(fontHeading, dlgX + (DLG_W - tw) / 2, titleTextY,
                         upperTitle.c_str(), false);
      currY += titleBarH;
    }

    // Vertical separator line down to footer
    int lineH = (dispH - NeuStyle::FOOTER_H) - (dlgY + DLG_H);
    if (lineH > 0) {
      const int sep_W = NeuStyle::SHADOW_OFFSET + NeuStyle::BORDER_W;
      renderer->fillRect(dispW / 2 - sep_W / 2, dlgY + DLG_H, sep_W, lineH,
                         true);
    }

    // Draw menu items
    std::vector<RenderedMenuItem> renderedItems;
    for (size_t p = 0; p < pageWrappedLines.size(); ++p) {
      int itemIdx = startIndex + static_cast<int>(p);
      int itemH = pageItemHeights[p];
      bool isSelected = (itemIdx == selectedIndex);
      const auto &lines = pageWrappedLines[p];
      int textBlockH = static_cast<int>(lines.size()) * bodyLineH;
      int textOffsetY = (itemH - textBlockH) / 2;

      if (isSelected) {
        // Black highlight box
        renderer->fillRect(innerX, currY, innerW, itemH, true);

        // White triangle icon
        int iconSize = 10;
        int iconY = currY + textOffsetY + (bodyLineH - iconSize) / 2;
        renderer->drawTriangleIcon(innerX + 12, iconY, iconSize, false);

        // White text
        for (size_t l = 0; l < lines.size(); ++l) {
          int textY = currY + textOffsetY + static_cast<int>(l) * bodyLineH;
          renderer->drawText(fontBody, innerX + 32, textY, lines[l].c_str(),
                             false);
        }
      } else {
        // White interior background
        // renderer->fillRect(innerX, currY, innerW, itemH, false);

        // Black text
        for (size_t l = 0; l < lines.size(); ++l) {
          int textY = currY + textOffsetY + static_cast<int>(l) * bodyLineH;
          renderer->drawText(fontBody, innerX + 32, textY, lines[l].c_str(),
                             true);
        }
      }

      renderedItems.push_back({innerX, currY, innerW, itemH, itemIdx});
      currY += itemH;
    }

    // Render footer
    FooterWidget footer;
    footer.btnBack = {true, "BACK", "Back", false};
    footer.btnConfirm = {true, "SELECT", "Select", true}; // Pill
    footer.btnPrev = {true, "PREV", "Prev", false};
    footer.btnNext = {true, "NEXT", "Next", false};
    footer.render(renderer, dispW, dispH);

    display.present();

    // Input loop
    bool stateChanged = false;
    while (!stateChanged) {
      ButtonEvent ev = input.pollInput();
      int touchX = -1, touchY = -1;

      if (input.getTouchPosition(touchX, touchY) && touchX >= 0 &&
          touchY >= 0) {
        input.resetActivityTimer();

        // Check footer touch
        ButtonEvent footerEv =
            footer.getButtonEventAt(touchX, touchY, dispW, dispH);
        if (footerEv == ButtonEvent::BACK) {
          return -1;
        } else if (footerEv == ButtonEvent::CONFIRM ||
                   footerEv == ButtonEvent::RIGHT &&
                       footer.getSlotAt(touchX, touchY, dispW, dispH) == 1) {
          return selectedIndex;
        } else if (footerEv == ButtonEvent::LEFT) { // Slot 2: PREV
          selectedIndex =
              (selectedIndex > 0) ? selectedIndex - 1 : (totalItems - 1);
          stateChanged = true;
          break;
        } else if (footerEv == ButtonEvent::RIGHT) { // Slot 3: NEXT
          selectedIndex =
              (selectedIndex + 1 < totalItems) ? selectedIndex + 1 : 0;
          stateChanged = true;
          break;
        }

        // Check item touch
        for (const auto &ri : renderedItems) {
          if (touchX >= ri.x && touchX <= ri.x + ri.w && touchY >= ri.y &&
              touchY <= ri.y + ri.h) {
            return ri.itemIndex; // Tapping item selects and confirms
          }
        }
      }

      if (ev == ButtonEvent::QUIT || ev == ButtonEvent::BACK) {
        input.resetActivityTimer();
        return -1;
      } else if (ev == ButtonEvent::CONFIRM) {
        input.resetActivityTimer();
        return selectedIndex;
      } else if (ev == ButtonEvent::UP || ev == ButtonEvent::LEFT) {
        input.resetActivityTimer();
        selectedIndex =
            (selectedIndex > 0) ? selectedIndex - 1 : (totalItems - 1);
        stateChanged = true;
        break;
      } else if (ev == ButtonEvent::DOWN || ev == ButtonEvent::RIGHT) {
        input.resetActivityTimer();
        selectedIndex =
            (selectedIndex + 1 < totalItems) ? selectedIndex + 1 : 0;
        stateChanged = true;
        break;
      }

#ifdef PLATFORM_ESP32
      delay(16);
#endif
    }
  }
}
