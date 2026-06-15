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
#include <string>

class InkEngine
{
public:
    InkEngine(IDisplay& display, IInput& input, IStorage& storage);
    ~InkEngine();

    bool loadStory(const char* path);
    void update();
    bool isDone() const { return _state == State::DONE; }

private:
    IDisplay& _display;
    IInput&   _input;
    IStorage& _storage;

    ink::runtime::story*  _story   = nullptr;
    ink::runtime::runner  _runner  = nullptr;
    ink::runtime::globals _globals = nullptr;
    const unsigned char*  _storyBuf = nullptr;

    std::vector<std::string> _narrativeLines;
    size_t _oldTextLineCount = 0;

    static constexpr int MAX_CHOICES = 8;
    char  _choiceText[MAX_CHOICES][128] = {};
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

    void tickRunningText();
    void tickWaitingInput();
    void tickStoryEnded();
    void redraw();
    void collectChoices();
    void drawNarrativeArea();
    void drawChoiceArea();
};
