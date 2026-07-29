#pragma once

#include "../hal/IDisplay.h"
#include "../lib/GfxRenderer/src/GfxRenderer.h"
#include "NeuStyle.h"
#include <functional>

class ListItemWidget {
public:
  // Draws a neubrutalist card for list views.
  // If `selected` is true, it draws a shadow and shifts the card up and left.
  // Returns the interior bounds (x, y, w, h) via the callback `drawContents`.
  static void draw(GfxRenderer* r, int x, int y, int w, int h, bool selected, 
                   const std::function<void(int interiorX, int interiorY, int interiorW, int interiorH)>& drawContents) {
    if (!r) return;

    int cardX = x;
    int cardY = y;
    
    if (selected) {
      // Draw shadow at original position
      r->fillRect(cardX, cardY, w, h, true);
      // Offset card
      cardX -= NeuStyle::SHADOW_OFFSET / 2; // -5 px
      cardY -= NeuStyle::SHADOW_OFFSET / 2; // -5 px
    }

    // Clear card interior (always white)
    r->fillRect(cardX, cardY, w, h, false);

    // Draw card border
    for (int i = 0; i < NeuStyle::BORDER_W; i++) {
      r->drawRect(cardX + i, cardY + i, w - 2 * i, h - 2 * i, true);
    }

    // Call inner render lambda
    if (drawContents) {
      drawContents(cardX + NeuStyle::BORDER_W, cardY + NeuStyle::BORDER_W, 
                   w - 2 * NeuStyle::BORDER_W, h - 2 * NeuStyle::BORDER_W);
    }
  }
};
