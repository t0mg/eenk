#pragma once

#include "hal/IDisplay.h"
#include "os/AppSettings.h"
#include "ui/StoryMetadata.h"
#include <deque>
#include <vector>
#include <string>
#include <TextBlock.h>
#include <runner.h>

class InkStoryManager;

struct WrappedLine {
    TextBlock block;
    bool isOld = false;
    bool isImage = false;
    std::string imagePath = "";
    int imageHeight = 0;
    bool endOfParagraph = false;
    int getHeight(class GfxRenderer* renderer) const;
};

class InkDisplayManager {
public:
    InkDisplayManager(IDisplay& display);
    ~InkDisplayManager();

    void applySettings(const AppSettings& settings);
    void resolveAndApplyFont(const StoryMetadata& meta, const char* storyBase, const char* storyDir);

    void clearHistory();
    void addWrappedLine(const WrappedLine& line);
    void popOldestLine();
    size_t getHistorySize() const { return _wrappedLines.size(); }
    std::deque<WrappedLine>& getHistory() { return _wrappedLines; }
    void markHistoryOld();

    void collectChoices(ink::runtime::runner& runner);
    int getChoicesHeight() const;

    int getScrollY() const { return _scrollY; }
    void setScrollY(int scrollY) { _scrollY = scrollY; }
    int getMaxScrollY() const { return _maxScrollY; }
    void doAutoScroll(int newLinesCount, bool showChoices);

    void setScrollToBottom();
    void scrollToSelectedChoice();

    bool isChoicesVisible() const;
    bool isRevealComplete() const;

    enum class ChoiceVisualState {
        HIDDEN,
        FULL
    };
    ChoiceVisualState getChoiceVisualState(int index) const;

    void redraw(InkStoryManager& storyManager, AppSettings& settings, int& refreshCount);

    struct ChoiceLayout {
        int marginY;
        int choiceLineHeight;
        int choicePadding;
        int minChoiceHeight;
    };
    ChoiceLayout getChoiceLayout(class GfxRenderer* renderer) const;

    // Animation state
    void setRevealStarted(bool started) { _revealStarted = started; }
    bool getRevealStarted() const { return _revealStarted; }
    void setRevealStep(int step) { _revealStep = step; }
    int getRevealStep() const { return _revealStep; }
    
    int getNumChoices() const { return _numChoices; }
    void setSelectedChoice(int index) { _selectedChoice = index; }
    int getSelectedChoice() const { return _selectedChoice; }
    void setBlinkingChoice(int index) { _blinkingChoice = index; }
    int getBlinkingChoice() const { return _blinkingChoice; }
    void flashChoiceActivation(InkStoryManager& storyManager, AppSettings& settings, int choiceIdx);
    
    // Y-coordinate mapping for touch interactions
    int getChoiceIndexAtY(int y) const;
    void setupStoryEndedChoices();

private:
    IDisplay& _display;
    AppSettings _settings = AppSettings::defaults();
    class StreamingEpdFontFamily* _streamingFamily = nullptr;
    
    std::deque<WrappedLine> _wrappedLines;
    
    static constexpr int MAX_CHOICES = 8;
    char  _choiceText[MAX_CHOICES][128] = {};
    std::vector<TextBlock> _wrappedChoices[MAX_CHOICES];
    int   _numChoices       = 0;
    int   _selectedChoice   = 0;
    int   _blinkingChoice   = -1;

    int _scrollY = 0;
    int _maxScrollY = 0;

    int _revealStep = 0;
    bool _revealStarted = false;
};
