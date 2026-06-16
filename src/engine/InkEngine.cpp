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

#ifndef SERIAL_DEBUG
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
#endif // !SERIAL_DEBUG

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

#ifndef SERIAL_DEBUG
    GfxRenderer* renderer = _display.getRenderer();
    if (renderer) {
        renderer->insertFont(FONT_NARRATIVE, fontNarrativeFamily);
        renderer->insertFont(FONT_CHOICE, fontChoiceFamily);
    }
#endif

    _wrappedLines.clear();
    _scrollY = 0;
    _maxScrollY = 0;
    _state = State::RUNNING_TEXT;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool InkEngine::loadStoryFromMemory(const unsigned char* data, std::size_t size)
{
    // NOTE: _storyBuf is NOT set — we don't own this memory (it's mmap'd)
    _story = ink::runtime::story::from_binary(data, size, false);
    if (!_story) {
        fprintf(stderr, "[InkEngine] story::from_binary() failed (mmap)\n");
        return false;
    }

    _globals = _story->new_globals();
    _runner  = _story->new_runner(_globals);

    printf("[InkEngine] Story loaded from memory — %zu bytes\n", size);

#ifndef SERIAL_DEBUG
    GfxRenderer* renderer = _display.getRenderer();
    if (renderer) {
        renderer->insertFont(FONT_NARRATIVE, fontNarrativeFamily);
        renderer->insertFont(FONT_CHOICE, fontChoiceFamily);
    }
#endif

    _wrappedLines.clear();
    _scrollY = 0;
    _maxScrollY = 0;
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
    GfxRenderer* renderer = _display.getRenderer();
    int narrativeWidth = _display.getWidth() - 48; // 2 * marginX
    int newLinesCount = 0;

    auto pushLine = [&](const std::string& str) {
        if (str.empty()) {
            _wrappedLines.push_back({"", false});
            newLinesCount++;
        } else if (renderer) {
            auto wraps = renderer->wrapTextWithHyphenation(FONT_NARRATIVE, str.c_str(), narrativeWidth, 100);
            for (auto& w : wraps) {
                _wrappedLines.push_back({w, false});
                newLinesCount++;
            }
        } else {
            _wrappedLines.push_back({str, false});
            newLinesCount++;
        }
    };

    while (_runner->can_continue()) {
        const char* line = _runner->getline_alloc();
        if (line) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
                s.pop_back();
            }
            pushLine(s);
        }
    }

    while (_wrappedLines.size() > 300) {
        _wrappedLines.pop_front();
    }

    auto doAutoScroll = [&]() {
        if (!renderer) return;
        int height = _display.getHeight();
        int marginY = 24;
        int choiceHeight = (_numChoices > 0) ? ((_numChoices * renderer->getLineHeight(FONT_CHOICE)) + marginY) : 0;
        int documentHeight = _wrappedLines.size() * renderer->getLineHeight(FONT_NARRATIVE) + choiceHeight;
        int availableHeight = height - (2 * marginY);
        
        _maxScrollY = std::max(0, documentHeight - availableHeight);
        
        int newContentHeight = newLinesCount * renderer->getLineHeight(FONT_NARRATIVE) + choiceHeight;
        if (newContentHeight > availableHeight) {
            // Scroll so the first new line is at the top of the visible narrative area
            int oldLinesCount = _wrappedLines.size() - newLinesCount;
            _scrollY = std::min(_maxScrollY, oldLinesCount * renderer->getLineHeight(FONT_NARRATIVE));
        } else {
            // Snap to bottom
            _scrollY = _maxScrollY;
        }

        if (_numChoices > 0 && _scrollY > _maxScrollY - choiceHeight - marginY) {
            _scrollY = _maxScrollY;
        }
    };

    if (_runner->has_choices()) {
        collectChoices();
        doAutoScroll();
        _state = State::SHOWING_CHOICES;
        redraw();
    } else {
        pushLine("--- THE END ---");
        pushLine("[Press ENTER to restart, ESC to quit]");
        doAutoScroll();
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

    bool choicesVisible = false;
    GfxRenderer* renderer = _display.getRenderer();
    if (renderer && _numChoices > 0) {
        int marginY = 24;
        int choiceHeight = (_numChoices * renderer->getLineHeight(FONT_CHOICE)) + marginY;
        if (_scrollY > _maxScrollY - choiceHeight - marginY) {
            choicesVisible = true;
        }
    } else if (!renderer) {
        choicesVisible = true;
    }

    if (!choicesVisible) {
        if (ev == ButtonEvent::UP) ev = ButtonEvent::LEFT;
        if (ev == ButtonEvent::DOWN) ev = ButtonEvent::RIGHT;
    }

    switch (ev) {
    case ButtonEvent::UP:
        if (_selectedChoice > 0) --_selectedChoice;
        else if (_numChoices > 0) _selectedChoice = _numChoices - 1;
        redraw();
        break;
    case ButtonEvent::DOWN:
        if (_selectedChoice < _numChoices - 1) ++_selectedChoice;
        else if (_numChoices > 0) _selectedChoice = 0;
        redraw();
        break;
    case ButtonEvent::LEFT: {
        int scrollAmount = _display.getHeight() / 4;
        _scrollY -= scrollAmount;
        if (_scrollY < 0) _scrollY = 0;
        redraw();
        break;
    }
    case ButtonEvent::RIGHT: {
        int scrollAmount = _display.getHeight() / 4;
        _scrollY += scrollAmount;
        
        GfxRenderer* renderer = _display.getRenderer();
        if (renderer && _numChoices > 0) {
            int marginY = 24;
            int choiceHeight = (_numChoices * renderer->getLineHeight(FONT_CHOICE)) + marginY;
            if (_scrollY > _maxScrollY - choiceHeight - marginY) {
                _scrollY = _maxScrollY;
            }
        }
        
        if (_scrollY > _maxScrollY) _scrollY = _maxScrollY;
        redraw();
        break;
    }
    case ButtonEvent::CONFIRM:
        if (_numChoices > 0) {
            for (auto& l : _wrappedLines) l.isOld = true;
            _runner->choose(static_cast<std::size_t>(_selectedChoice));
            _state = State::RUNNING_TEXT;
        }
        break;
    case ButtonEvent::BACK:
        _wrappedLines.push_back({"[Save system coming in Milestone 5]", false});
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
        _wrappedLines.clear();
        _scrollY = 0;
        _maxScrollY = 0;
        _state = State::RUNNING_TEXT;
    } else if (ev == ButtonEvent::LEFT) {
        int scrollAmount = _display.getHeight() / 4;
        _scrollY -= scrollAmount;
        if (_scrollY < 0) _scrollY = 0;
        redraw();
    } else if (ev == ButtonEvent::RIGHT) {
        int scrollAmount = _display.getHeight() / 4;
        _scrollY += scrollAmount;
        
        GfxRenderer* renderer = _display.getRenderer();
        if (renderer && _numChoices > 0) {
            int marginY = 24;
            int choiceHeight = (_numChoices * renderer->getLineHeight(FONT_CHOICE)) + marginY;
            if (_scrollY > _maxScrollY - choiceHeight - marginY) {
                _scrollY = _maxScrollY;
            }
        }
        
        if (_scrollY > _maxScrollY) _scrollY = _maxScrollY;
        redraw();
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
        // ── Text-mode (e.g. EspSerialDisplay) ───────────────────────────────
        for (const auto& line : _wrappedLines) {
            _display.drawNarrativeLine(line.text.c_str());
        }
        if (_numChoices > 0) {
            _display.drawSeparator();
            for (int i = 0; i < _numChoices; ++i) {
                _display.drawChoiceLine(i, _choiceText[i], i == _selectedChoice);
            }
        }
        _display.present();
        return;
    }

#ifndef SERIAL_DEBUG
    int width = _display.getWidth();
    int height = _display.getHeight();

    int marginX = 24;
    int marginY = 24;
    int narrativeWidth = width - (2 * marginX);

    int lineHeight = renderer->getLineHeight(FONT_NARRATIVE);
    int choiceLineHeight = renderer->getLineHeight(FONT_CHOICE);
    
    int y = marginY - _scrollY;

    for (const auto& w : _wrappedLines) {
        if (!w.text.empty() && (y + lineHeight > 0) && (y < height)) {
            renderer->setHalftone(w.isOld);
            renderer->drawText(FONT_NARRATIVE, marginX, y, w.text.c_str());
            renderer->setHalftone(false);
        }
        y += lineHeight;
    }

    if (_numChoices > 0) {
        y += (marginY / 2);
        
        if ((y + 2 > 0) && (y < height)) {
            renderer->fillRect(marginX, y, narrativeWidth, 2, true);
        }
        y += (marginY / 2);

        for (int i = 0; i < _numChoices; ++i) {
            bool selected = (i == _selectedChoice);
            char buf[256];
            snprintf(buf, sizeof(buf), "%s %s", selected ? ">" : " ", _choiceText[i]);

            if ((y + choiceLineHeight > 0) && (y < height)) {
                if (selected) {
                    renderer->fillRect(marginX - 4, y, narrativeWidth + 8, choiceLineHeight, true);
                    renderer->drawText(FONT_CHOICE, marginX, y, buf, false);
                } else {
                    renderer->drawText(FONT_CHOICE, marginX, y, buf, true);
                }
            }
            y += choiceLineHeight;
        }
    }

    _display.present();
#endif // !SERIAL_DEBUG
}
