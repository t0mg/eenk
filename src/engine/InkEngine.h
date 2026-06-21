#pragma once
#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "hal/IStorage.h"

// <cmath> must be included at global scope before InkCPP headers so GCC 15's
// specfun.h special math functions (sph_bessel, etc.) are not injected into
// ink::runtime namespace.
#include <cmath>

// InkCPP types (story_ptr smart handles)
#include <story.h>
#include <runner.h>
#include <globals.h>

#include <cstddef>
#include <vector>
#include <deque>
#include <string>

class InkEngine
{
public:
    InkEngine(IDisplay& display, IInput& input, IStorage& storage);
    ~InkEngine();

    bool loadStory(const char* path);
    bool loadStoryFromMemory(const unsigned char* data, std::size_t size);
    
    // Save/Load system
    const unsigned char* createSnapshot(std::size_t* outLength);
    void freeSnapshot();
    bool loadSnapshot(const unsigned char* data, std::size_t length);
    
    void update();
    bool isDone() const { return _state == State::DONE; }
    bool shouldSleep() const { return _shouldSleep; }

    struct WrappedLine {
        std::string text;
        bool isOld;
    };
    const std::deque<WrappedLine>& getHistory() const { return _wrappedLines; }
    void setHistory(const std::deque<WrappedLine>& history) { _wrappedLines = history; }

private:
    IDisplay& _display;
    IInput&   _input;
    IStorage& _storage;

    ink::runtime::story*  _story   = nullptr;
    ink::runtime::runner  _runner;
    ink::runtime::snapshot* _currentSnapshot = nullptr;
    ink::runtime::globals _globals = nullptr;
    const unsigned char*  _storyBuf = nullptr;

    std::deque<WrappedLine> _wrappedLines;
    int _scrollY = 0;
    int _maxScrollY = 0;

    static constexpr int MAX_CHOICES = 8;
    char  _choiceText[MAX_CHOICES][128] = {};
    std::vector<std::string> _wrappedChoices[MAX_CHOICES];
    int   _numChoices     = 0;
    int   _selectedChoice = 0;

    enum class State {
        IDLE,
        RUNNING_TEXT,
        SHOWING_CHOICES,
        WAITING_INPUT,
        SAVE_STUB,
        STORY_ENDED,
        DONE,
    };
    State _state = State::IDLE;
    bool _shouldSleep = false;

    void tickRunningText();
    void tickWaitingInput();
    void tickStoryEnded();
    void redraw();
    void collectChoices();
    void drawNarrativeArea();
    void drawChoiceArea();
    int getChoicesHeight(class GfxRenderer* renderer) const;
};
