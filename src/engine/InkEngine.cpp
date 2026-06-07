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

#include <GfxRenderer.h>
#include <EpdFont.h>
#include <EpdFontFamily.h>

// Builtin fonts
#include <builtinFonts/reader_medium_2b.h>
#include <builtinFonts/ui_12.h>

static constexpr int FONT_NARRATIVE = 1;
static constexpr int FONT_CHOICE = 2;

static EpdFont fontNarrativeData(&reader_medium_2b);
static EpdFont fontChoiceData(&ui_12);
static EpdFontFamily fontNarrativeFamily(&fontNarrativeData);
static EpdFontFamily fontChoiceFamily(&fontChoiceData);

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

InkEngine::InkEngine(IDisplay& display, IInput& input, IStorage& storage)
    : _display(display), _input(input), _storage(storage)
{}

InkEngine::~InkEngine()
{
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

    _story = ink::runtime::story::from_binary(_storyBuf, size, false);
    if (!_story) {
        fprintf(stderr, "[InkEngine] story::from_binary() failed\n");
        _storage.freeBuffer(_storyBuf);
        _storyBuf = nullptr;
        return false;
    }

    _globals = _story->new_globals();
    _runner  = _story->new_runner(_globals);

    printf("[InkEngine] Story loaded — %zu bytes\n", size);

    GfxRenderer* renderer = _display.getRenderer();
    if (renderer) {
        renderer->insertFont(FONT_NARRATIVE, fontNarrativeFamily);
        renderer->insertFont(FONT_CHOICE, fontChoiceFamily);
    }

    _narrativeLines.clear();
    _state = State::RUNNING_TEXT;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void InkEngine::update()
{
    switch (_state) {
    case State::RUNNING_TEXT:
        tickRunningText();
        break;
    case State::SHOWING_CHOICES:
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
        _state = State::WAITING_INPUT;
        redraw();
        break;
    case State::DONE:
    case State::IDLE:
        break;
    }

#ifdef PLATFORM_NATIVE
    SDL_Delay(16); // cap at ~60 fps
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void InkEngine::tickRunningText()
{
    while (_runner->can_continue()) {
        const char* line = _runner->getline_alloc();
        if (line) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
                s.pop_back();
            }
            _narrativeLines.push_back(s);
        }
    }

    if (_runner->has_choices()) {
        collectChoices();
        _state = State::SHOWING_CHOICES;
        redraw();
    } else {
        _narrativeLines.push_back("--- THE END ---");
        _narrativeLines.push_back("[Press ENTER to restart, ESC to quit]");
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
            strncpy(_choiceText[_numChoices], txt, sizeof(_choiceText[0]) - 1);
            _choiceText[_numChoices][sizeof(_choiceText[0]) - 1] = '\0';
        } else {
            _choiceText[_numChoices][0] = '\0';
        }
        ++_numChoices;
    }
}

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
        _narrativeLines.push_back("[Save system coming in Milestone 5]");
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
        _narrativeLines.clear();
        _state = State::RUNNING_TEXT;
    } else if (ev == ButtonEvent::QUIT) {
        _state = State::DONE;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void InkEngine::redraw()
{
    _display.clear();
    
    GfxRenderer* renderer = _display.getRenderer();
    if (!renderer) {
        _display.present();
        return;
    }

    int width = _display.getWidth();
    int height = _display.getHeight();

    int marginX = 24;
    int marginY = 24;
    int narrativeWidth = width - (2 * marginX);

    std::vector<std::string> wrappedLines;
    for (const auto& line : _narrativeLines) {
        if (line.empty()) {
            wrappedLines.push_back("");
            continue;
        }
        auto wraps = renderer->wrapTextWithHyphenation(FONT_NARRATIVE, line.c_str(), narrativeWidth, 100);
        for (const auto& w : wraps) {
            wrappedLines.push_back(w);
        }
    }

    int lineHeight = renderer->getLineHeight(FONT_NARRATIVE);
    int choiceLineHeight = renderer->getLineHeight(FONT_CHOICE);
    int choiceHeight = 0;

    if (_numChoices > 0) {
        choiceHeight = (_numChoices * choiceLineHeight) + marginY;
    }
    
    int availableHeightForNarrative = height - (2 * marginY) - choiceHeight;
    int maxVisibleRows = availableHeightForNarrative / lineHeight;

    int startIdx = 0;
    if ((int)wrappedLines.size() > maxVisibleRows) {
        startIdx = wrappedLines.size() - maxVisibleRows;
    }

    int y = marginY;
    for (size_t i = startIdx; i < wrappedLines.size(); ++i) {
        if (!wrappedLines[i].empty()) {
            renderer->drawText(FONT_NARRATIVE, marginX, y, wrappedLines[i].c_str());
        }
        y += lineHeight;
    }

    if (_numChoices > 0) {
        y += (marginY / 2);
        renderer->fillRect(marginX, y, narrativeWidth, 2, true);
        y += (marginY / 2);
        
        for (int i = 0; i < _numChoices; ++i) {
            bool selected = (i == _selectedChoice);
            char buf[256];
            snprintf(buf, sizeof(buf), "%s %d. %s", selected ? ">" : " ", i + 1, _choiceText[i]);
            
            if (selected) {
                renderer->fillRect(marginX - 4, y, narrativeWidth + 8, choiceLineHeight, true);
                renderer->drawText(FONT_CHOICE, marginX, y, buf, false);
            } else {
                renderer->drawText(FONT_CHOICE, marginX, y, buf, true);
            }
            y += choiceLineHeight;
        }
    }

    _display.present();
}
