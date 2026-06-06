#pragma once
#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "hal/IStorage.h"
#include "engine/TextFormatter.h"

// <cmath> must be included at global scope before InkCPP headers so GCC 15's
// specfun.h special math functions (sph_bessel, etc.) are not injected into
// ink::runtime namespace.
#include <cmath>

// InkCPP types (story_ptr smart handles)
#include <story.h>
#include <runner.h>
#include <globals.h>

#include <cstddef>

/**
 * EENK — InkEngine
 *
 * Core game-loop controller.  Owns the InkCPP story/runner lifecycle and
 * drives rendering via the HAL interfaces.
 *
 * State machine:
 *
 *   IDLE ──loadStory()──► RUNNING_TEXT
 *   RUNNING_TEXT ─────────────────────► SHOWING_CHOICES
 *                                           │
 *                         ◄─ CONFIRM ───────┤
 *                                           ▼
 *                                       WAITING_INPUT
 *                                           │
 *                              (UP/DOWN)  ──┤── (CONFIRM) ──► RUNNING_TEXT
 *                                           │
 *                              (BACK)  ────►│──► SAVE_STUB
 *                                           │
 *                              (QUIT)  ────►│──► DONE
 *   RUNNING_TEXT ─ no more content ────────►│──► DONE
 */
class InkEngine
{
public:
    InkEngine(IDisplay& display, IInput& input, IStorage& storage);
    ~InkEngine();

    /**
     * Load a compiled .bin story file from storage.
     * @return true on success, false if the file cannot be read or parsed.
     */
    bool loadStory(const char* path);

    /**
     * Advance the game loop by one tick.
     * Call this in a tight loop until isDone() returns true.
     * Internally pumps input, updates state, and redraws the display.
     */
    void update();

    /** Returns true when the story has ended or the user quit. */
    bool isDone() const { return _state == State::DONE; }

private:
    // ── Dependencies ─────────────────────────────────────────────
    IDisplay& _display;
    IInput&   _input;
    IStorage& _storage;

    // ── InkCPP objects ───────────────────────────────────────────
    ink::runtime::story*  _story   = nullptr;
    ink::runtime::runner  _runner  = nullptr;
    ink::runtime::globals _globals = nullptr;
    const unsigned char*  _storyBuf = nullptr; // heap buffer for story binary

    // ── Formatter / scroll buffer ─────────────────────────────────
    TextFormatter _formatter;

    // ── Choice state ──────────────────────────────────────────────
    static constexpr int MAX_CHOICES = 8;
    char  _choiceText[MAX_CHOICES][128] = {};
    int   _numChoices     = 0;
    int   _selectedChoice = 0;

    // ── State machine ─────────────────────────────────────────────
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

    // ── Internal helpers ──────────────────────────────────────────
    void tickRunningText();
    void tickWaitingInput();
    void tickStoryEnded();
    void redraw();
    void collectChoices();
    void drawNarrativeArea();
    void drawChoiceArea();
    int  narrativeRows() const;
};
