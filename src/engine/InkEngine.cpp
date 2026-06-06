// Include <cmath> FIRST so GCC 15's specfun.h special-math functions are
// emitted at global scope rather than inside ink::runtime namespace.
#include <cmath>

#include "engine/InkEngine.h"

// InkCPP public API (already via InkEngine.h, but choice.h is also needed)
#include <choice.h>

#include <cstdio>
#include <cstring>
#include <cctype>

#ifdef PLATFORM_NATIVE
#include <SDL.h>  // for SDL_Delay
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

InkEngine::InkEngine(IDisplay& display, IInput& input, IStorage& storage)
    : _display(display), _input(input), _storage(storage)
{}

InkEngine::~InkEngine()
{
    // InkCPP smart handles manage runner/globals lifetime
    // We only manually free the story binary buffer
    if (_story) {
        delete _story;
        _story = nullptr;
    }
    if (_storyBuf) {
        _storage.freeBuffer(_storyBuf);
        _storyBuf = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
bool InkEngine::loadStory(const char* path)
{
    std::size_t size = 0;
    _storyBuf = _storage.readFileBinary(path, &size);
    if (!_storyBuf || size == 0) {
        fprintf(stderr, "[InkEngine] Failed to read: %s\n", path);
        return false;
    }

    _story = ink::runtime::story::from_binary(_storyBuf, size,
                                               /*freeOnDestroy=*/false);
    if (!_story) {
        fprintf(stderr, "[InkEngine] story::from_binary() failed\n");
        _storage.freeBuffer(_storyBuf);
        _storyBuf = nullptr;
        return false;
    }

    _globals = _story->new_globals();
    _runner  = _story->new_runner(_globals);

    printf("[InkEngine] Story loaded — %zu bytes\n", size);

    _formatter.clear();
    _state = State::RUNNING_TEXT;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main update tick
// ─────────────────────────────────────────────────────────────────────────────

void InkEngine::update()
{
    switch (_state) {
    case State::RUNNING_TEXT:
        tickRunningText();
        break;
    case State::SHOWING_CHOICES:
        // Immediately transition — SHOWING_CHOICES only exists for one tick
        // to give the redraw a clean entry point before waiting.
        _state = State::WAITING_INPUT;
        redraw();
        break;
    case State::WAITING_INPUT:
        tickWaitingInput();
        break;
    case State::STORY_ENDED:
        tickStoryEnded();
        break;
    case State::SAVE_STUB:
        // Stub: just show a message and go back
        // (real save system in Milestone 5)
        _state = State::WAITING_INPUT;
        redraw();
        break;
    case State::DONE:
    case State::IDLE:
        break;
    }

#ifdef PLATFORM_NATIVE
    SDL_Delay(16); // cap at ~60 fps to avoid spinning the CPU
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Drain all available text from the runner into the formatter
// ─────────────────────────────────────────────────────────────────────────────

void InkEngine::tickRunningText()
{
    // Drain as many lines as the runner will give us this tick
    bool gotAny = false;
    while (_runner->can_continue()) {
        const char* line = _runner->getline_alloc();
        if (line) {
            _formatter.append(line);
            gotAny = true;
        }
    }

    if (_runner->has_choices()) {
        collectChoices();
        _state = State::SHOWING_CHOICES;
        redraw();
    } else {
        // Story ended
        _formatter.append("--- THE END ---");
        _formatter.append("[Press ENTER to restart, ESC to quit]");
        _state = State::STORY_ENDED;
        redraw();
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void InkEngine::collectChoices()
{
    _numChoices     = 0;
    _selectedChoice = 0;
    for (const auto* c = _runner->begin(); c != _runner->end(); ++c) {
        if (_numChoices >= MAX_CHOICES) break;
        const char* txt = c->text();
        if (txt) {
            strncpy(_choiceText[_numChoices], txt,
                    sizeof(_choiceText[0]) - 1);
            _choiceText[_numChoices][sizeof(_choiceText[0]) - 1] = '\0';
        } else {
            _choiceText[_numChoices][0] = '\0';
        }
        ++_numChoices;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Handle user input
// ─────────────────────────────────────────────────────────────────────────────

void InkEngine::tickWaitingInput()
{
    ButtonEvent ev = _input.pollInput();
    switch (ev) {
    case ButtonEvent::UP:
        if (_selectedChoice > 0) --_selectedChoice;
        redraw();
        break;
    case ButtonEvent::DOWN:
        if (_selectedChoice < _numChoices - 1) ++_selectedChoice;
        redraw();
        break;
    case ButtonEvent::CONFIRM:
        if (_numChoices > 0) {
            _runner->choose(static_cast<std::size_t>(_selectedChoice));
            _state = State::RUNNING_TEXT;
        }
        break;
    case ButtonEvent::BACK:
        // Stub: acknowledge with a message on screen
        _formatter.append("[Save system coming in Milestone 5]");
        _state = State::SAVE_STUB;
        break;
    case ButtonEvent::QUIT:
        _state = State::DONE;
        break;
    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void InkEngine::tickStoryEnded()
{
    ButtonEvent ev = _input.pollInput();
    if (ev == ButtonEvent::CONFIRM) {
        _globals = _story->new_globals();
        _runner  = _story->new_runner(_globals);
        _formatter.clear();
        _state = State::RUNNING_TEXT;
    } else if (ev == ButtonEvent::QUIT) {
        _state = State::DONE;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering
// ─────────────────────────────────────────────────────────────────────────────

int InkEngine::narrativeRows() const
{
    // Reserve rows for choices + 1 divider line + 1 blank gap
    int reserved = (_numChoices > 0) ? _numChoices + 2 : 0;
    return _display.getRows() - reserved;
}

void InkEngine::redraw()
{
    _display.clear();
    drawNarrativeArea();
    if (_numChoices > 0) {
        drawChoiceArea();
    }
    _display.present();
}

void InkEngine::drawNarrativeArea()
{
    int narRows = narrativeRows();
    int start, count;
    _formatter.getVisibleWindow(narRows, &start, &count);

    for (int i = 0; i < count; ++i) {
        const char* line = _formatter.getLine(start + i);
        if (line) {
            _display.drawText(0, i, line, /*inverted=*/false);
        }
    }
}

void InkEngine::drawChoiceArea()
{
    int totalRows  = _display.getRows();
    int dividerRow = totalRows - _numChoices - 1;

    // Draw divider line
    int dividerY = _display.getLineHeight() * dividerRow
                 + _display.getLineHeight() / 2;
    _display.drawHLine(dividerY);

    // Draw each choice
    for (int i = 0; i < _numChoices; ++i) {
        int row = dividerRow + 1 + i;
        bool selected = (i == _selectedChoice);

        // Build choice label: "> text" or "  text"
        char buf[FMT_MAX_LINE_LEN];
        snprintf(buf, sizeof(buf), "%s %d. %s",
                 selected ? ">" : " ",
                 i + 1,
                 _choiceText[i]);

        _display.drawText(0, row, buf, selected);
    }
}
