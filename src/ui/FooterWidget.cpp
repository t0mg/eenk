#include "FooterWidget.h"
#include "NeuStyle.h"
#include <GfxRenderer.h>
#include <cctype>

void FooterWidget::render(GfxRenderer* r, int displayWidth, int displayHeight) const {
    if (!r) return;

    int startY = displayHeight - HEIGHT;
    
    // Draw top separator line (black, BORDER_W thick)
    r->fillRect(0, startY, displayWidth, NeuStyle::BORDER_W, true);

    // Fill footer background below separator to black
    r->fillRect(0, startY + NeuStyle::BORDER_W, displayWidth, HEIGHT - NeuStyle::BORDER_W, true);

    const FooterAction* actions[4] = {&btnBack, &btnConfirm, &btnPrev, &btnNext};

    // When only outer slots 0 and 3 are active (e.g. 2-choice confirm dialog),
    // keep middle slots 1 & 2 empty and equalize left and right outer margins.
    bool isOnlyOuterTwo = (actions[0]->hasAction && actions[3]->hasAction &&
                           !actions[1]->hasAction && !actions[2]->hasAction);

    int fontIndex = NeuStyle::FONT_HEADING;
    int textH = r->getLineHeight(fontIndex);
    int contentTop = startY + NeuStyle::BORDER_W;
    int contentH = HEIGHT - NeuStyle::BORDER_W - BEZEL_OFFSET_Y;

    if (isOnlyOuterTwo) {
        // Convert labels to uppercase
        std::string label0 = actions[0]->customLabel.empty() ? actions[0]->defaultLabel : actions[0]->customLabel;
        std::string label3 = actions[3]->customLabel.empty() ? actions[3]->defaultLabel : actions[3]->customLabel;
        for (auto& c : label0) c = toupper(c);
        for (auto& c : label3) c = toupper(c);

        std::string trunc0 = r->truncatedText(fontIndex, label0.c_str(), MAX_LABEL_WIDTH);
        std::string trunc3 = r->truncatedText(fontIndex, label3.c_str(), MAX_LABEL_WIDTH);

        int y = contentTop + (contentH - textH) / 2;

        // Slot 0 (left choice)
        if (actions[0]->isPill) {
            int pillY = contentTop + (contentH - NeuStyle::PILL_H) / 2;
            // Pill is inverted=false (white background with black text) since footer is black
            r->drawPill(fontIndex, NeuStyle::MARGIN_X, pillY, trunc0.c_str(),
                        NeuStyle::PILL_PADDING_X, NeuStyle::PILL_H, NeuStyle::PILL_RADIUS, false);
        } else {
            // Normal text is white
            r->drawText(fontIndex, NeuStyle::MARGIN_X, y, trunc0.c_str(), false);
        }

        // Slot 3 (right choice)
        if (actions[3]->isPill) {
            int pillY = contentTop + (contentH - NeuStyle::PILL_H) / 2;
            int pillW = r->getTextWidth(fontIndex, trunc3.c_str()) + 2 * NeuStyle::PILL_PADDING_X + 2 * NeuStyle::PILL_RADIUS;
            int pillX = displayWidth - NeuStyle::MARGIN_X - pillW;
            r->drawPill(fontIndex, pillX, pillY, trunc3.c_str(),
                        NeuStyle::PILL_PADDING_X, NeuStyle::PILL_H, NeuStyle::PILL_RADIUS, false);
        } else {
            int w3 = r->getTextWidth(fontIndex, trunc3.c_str());
            int x3 = displayWidth - NeuStyle::MARGIN_X - w3;
            r->drawText(fontIndex, x3, y, trunc3.c_str(), false);
        }

        return;
    }

    // Standard 4 immovable slots layout
    for (int i = 0; i < 4; ++i) {
        if (actions[i]->hasAction) {
            std::string label = actions[i]->customLabel;
            if (label.empty()) {
                label = actions[i]->defaultLabel;
            }
            // Convert to uppercase
            for (auto& c : label) c = toupper(c);

            // Truncate if > MAX_LABEL_WIDTH
            std::string truncLabel = r->truncatedText(fontIndex, label.c_str(), MAX_LABEL_WIDTH);

            if (actions[i]->isPill) {
                int pillY = contentTop + (contentH - NeuStyle::PILL_H) / 2;
                int pillW = r->getTextWidth(fontIndex, truncLabel.c_str()) + 2 * NeuStyle::PILL_PADDING_X + 2 * NeuStyle::PILL_RADIUS;
                int pillX = i * BTN_WIDTH + (BTN_WIDTH - pillW) / 2;
                r->drawPill(fontIndex, pillX, pillY, truncLabel.c_str(),
                            NeuStyle::PILL_PADDING_X, NeuStyle::PILL_H, NeuStyle::PILL_RADIUS, false);
            } else {
                int textW = r->getTextWidth(fontIndex, truncLabel.c_str());
                int x = i * BTN_WIDTH + (BTN_WIDTH - textW) / 2;
                int y = contentTop + (contentH - textH) / 2;
                r->drawText(fontIndex, x, y, truncLabel.c_str(), false);
            }
        }
    }
}
