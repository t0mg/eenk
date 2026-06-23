#include "FooterWidget.h"
#include <GfxRenderer.h>

void FooterWidget::render(GfxRenderer* r, int displayWidth, int displayHeight, int fontIndex) const {
    if (!r) return;

    int startY = displayHeight - HEIGHT;
    
    // Draw top separator line (black line, 2px thick)
    r->fillRect(0, startY, displayWidth, 2, true);

    // Clear footer background below separator to white
    r->fillRect(0, startY + 2, displayWidth, HEIGHT - 2, false);

    const FooterAction* actions[4] = {&btnBack, &btnConfirm, &btnPrev, &btnNext};

    for (int i = 0; i < 4; ++i) {
        if (actions[i]->hasAction) {
            std::string label = actions[i]->customLabel;
            if (label.empty()) {
                label = actions[i]->defaultLabel;
            }

            // Truncate if > MAX_LABEL_WIDTH
            std::string truncLabel = r->truncatedText(fontIndex, label.c_str(), MAX_LABEL_WIDTH);

            // Center horizontally and vertically
            int textW = r->getTextWidth(fontIndex, truncLabel.c_str());
            int textH = r->getLineHeight(fontIndex);

            int x = i * BTN_WIDTH + (BTN_WIDTH - textW) / 2;
            int y = startY + (HEIGHT - textH) / 2;

            r->drawText(fontIndex, x, y, truncLabel.c_str(), true);
        }
    }
}
