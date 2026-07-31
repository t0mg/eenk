#include "LoadingWidget.h"
#include <GfxRenderer.h>
#include "NeuStyle.h"
#include <cstdio>
#include <vector>
#include <string>
#include <vector>

void LoadingWidget::show(IDisplay &display, const char *title, float progress) {
  auto renderer = display.getRenderer();
  if (renderer) {
    display.clear();

    const int barWidth = 360; // Slightly wider for a loading bar
    const int barHeight = 80;
    const int barX = (display.getWidth() - barWidth) / 2;
    const int barY = (display.getHeight() - barHeight) / 2;

    // Background box
    renderer->drawShadowBox(barX, barY, barWidth, barHeight, NeuStyle::BORDER_W, NeuStyle::SHADOW_OFFSET, false);

    int maxFillW = barWidth - 2 * NeuStyle::BORDER_W;
    
    // Fill the inside with black (start full black)
    renderer->fillRect(barX + NeuStyle::BORDER_W, barY + NeuStyle::BORDER_W, maxFillW, barHeight - 2 * NeuStyle::BORDER_W, true);

    // Title centered
    int fontId = NeuStyle::FONT_HEADING;
    int textY = barY + (barHeight - renderer->getLineHeight(fontId)) / 2;
    std::string upperTitle(title ? title : "");
    for (auto &c : upperTitle) c = toupper(c);
    renderer->drawCenteredText(fontId, textY, upperTitle.c_str(), false); // False = white text

    // Progress invert section
    int fillWidth = (int)(maxFillW * progress);
    if (fillWidth > 0) {
      renderer->invertRect(barX + NeuStyle::BORDER_W, barY + NeuStyle::BORDER_W, fillWidth, barHeight - 2 * NeuStyle::BORDER_W);
    }

    display.present();
  } else {
    printf("\r%s: %d%%", title, (int)(progress * 100));
    fflush(stdout);
  }
}

void LoadingWidget::showMessage(IDisplay &display, const char *title, const char *message) {
  auto renderer = display.getRenderer();
  if (renderer) {
    display.clear();
    
    const int dispW = display.getWidth();
    const int dispH = display.getHeight();
    static constexpr int DLG_W = 400; // Wider than normal dialog for errors
    int fontHeading = NeuStyle::FONT_HEADING;
    int fontBody = NeuStyle::FONT_BODY;
    static constexpr int marginX = 20;
    int maxTextW = DLG_W - 2 * marginX;

    bool hasTitle = (title && title[0] != '\0');
    int headingLineH = hasTitle ? renderer->getLineHeight(fontHeading) : 0;
    int msgLineH = renderer->getLineHeight(fontBody);

    std::vector<std::string> wrappedLines;
    if (message && message[0] != '\0') {
      std::string msgStr(message);
      size_t start = 0;
      size_t end = msgStr.find('\n');
      while (end != std::string::npos) {
        std::string para = msgStr.substr(start, end - start);
        if (!para.empty()) {
          auto lines = renderer->wrapTextWithHyphenation(fontBody, para.c_str(), maxTextW, 20);
          wrappedLines.insert(wrappedLines.end(), lines.begin(), lines.end());
        } else {
          wrappedLines.push_back("");
        }
        start = end + 1;
        end = msgStr.find('\n', start);
      }
      std::string para = msgStr.substr(start);
      if (!para.empty()) {
        auto lines = renderer->wrapTextWithHyphenation(fontBody, para.c_str(), maxTextW, 20);
        wrappedLines.insert(wrappedLines.end(), lines.begin(), lines.end());
      }
    }

    int titleBarH = hasTitle ? (headingLineH + 32) : 0;
    int contentH = 20 + (wrappedLines.size() * msgLineH) + 30;
    int totalH = titleBarH + contentH;
    
    int dlgX = (dispW - DLG_W) / 2;
    int dlgY = (dispH - totalH) / 2;

    renderer->drawShadowBox(dlgX, dlgY, DLG_W, totalH, NeuStyle::BORDER_W, NeuStyle::SHADOW_OFFSET, false);
    
    int contentY = dlgY + NeuStyle::BORDER_W;
    if (hasTitle) {
      renderer->fillRect(dlgX + NeuStyle::BORDER_W, contentY, DLG_W - 2 * NeuStyle::BORDER_W, titleBarH - NeuStyle::BORDER_W, true);
      int titleTextY = contentY + (titleBarH - NeuStyle::BORDER_W - headingLineH) / 2;
      std::string upperTitle(title);
      for (auto &c : upperTitle) c = toupper(c);
      renderer->drawCenteredText(fontHeading, titleTextY, upperTitle.c_str(), false);
      contentY += titleBarH - NeuStyle::BORDER_W;
      
      renderer->fillRect(dlgX, contentY, DLG_W, NeuStyle::BORDER_W, true);
      contentY += NeuStyle::BORDER_W;
    }
    
    int y = contentY + 20;
    for (const auto &line : wrappedLines) {
      if (!line.empty()) {
        renderer->drawText(fontBody, dlgX + marginX, y, line.c_str(), true);
      }
      y += msgLineH;
    }

    display.fullRefresh();
  } else {
    printf("=== %s ===\n%s\n", title, message);
  }
}
