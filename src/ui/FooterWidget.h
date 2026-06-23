#pragma once
#include <string>

class GfxRenderer;

struct FooterAction {
    bool hasAction = false;
    std::string customLabel;
    std::string defaultLabel; // e.g. "Back", "Confirm", "Prev", "Next"
};

class FooterWidget {
public:
    static constexpr int BEZEL_OFFSET_Y = 4;
    static constexpr int HEIGHT = 48 + BEZEL_OFFSET_Y;
    static constexpr int BTN_WIDTH = 120;
    static constexpr int MAX_LABEL_WIDTH = 110;

    // Layout order: 0=Back, 1=Confirm, 2=Prev, 3=Next
    FooterAction btnBack;
    FooterAction btnConfirm;
    FooterAction btnPrev;
    FooterAction btnNext;

    // Renders the 48px tall footer at the bottom of the screen.
    void render(GfxRenderer* r, int displayWidth, int displayHeight, int fontIndex) const;
};
