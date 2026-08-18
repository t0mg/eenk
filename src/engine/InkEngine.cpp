// Include <cmath> FIRST so GCC 15's specfun.h special-math functions are
// emitted at global scope rather than inside ink::runtime namespace.
#include <SDCardManager.h>

#include "engine/InkEngine.h"

// InkCPP public API (already via InkEngine.h, but choice.h is also needed)
#include <choice.h>

#include "GfxRenderer.h"
#include "InkRichTextParser.h"
#include "os/AppSettings.h"
#include "ui/ImageWidget.h"
#include "ui/QuickMenuWidget.h"
#include "ui/SettingsView.h"
#include "ui/SystemUI.h"
#include <cstdio>
#include <cstring>
#include <snapshot.h>

#ifdef PLATFORM_NATIVE
#include "hal/sdl/mock/FS.h"
#include <SDL.h> // for SDL_Delay
#endif

extern int g_marginPx;
extern int g_refreshInterval;
int g_marginPx = 16;
int g_refreshInterval = 10;

InkEngine::InkEngine(IDisplay &display, IInput &input, IStorage &storage)
    : _display(display), _input(input), _storage(storage),
      _storyManager(storage), _displayManager(display), _inputHandler(input) {}

InkEngine::~InkEngine() {}

#ifdef PLATFORM_NATIVE
bool InkEngine::loadStory(const char *path) {
  StoryMetadata meta;
  std::string storyBase, storyDir;
  if (!_storyManager.loadStory(path, meta, storyBase, storyDir)) {
    return false;
  }

  _displayManager.resolveAndApplyFont(meta, storyBase.c_str(),
                                      storyDir.c_str());
  _storyManager.loadMediaSidecar((meta.flags & 1) != 0, path);

  _displayManager.clearHistory();
  _displayManager.setScrollY(0);
  _state = State::RUNNING_TEXT;
  return true;
}
#else
bool InkEngine::loadStory(const unsigned char *data, std::size_t size,
                          const char *storyPath) {
  StoryMetadata meta;
  std::string storyBase, storyDir;
  if (!_storyManager.loadStory(data, size, storyPath, meta, storyBase,
                               storyDir)) {
    return false;
  }

  _displayManager.resolveAndApplyFont(meta, storyBase.c_str(),
                                      storyDir.c_str());
  _storyManager.loadMediaSidecar((meta.flags & 1) != 0, storyPath);

  _displayManager.clearHistory();
  _displayManager.setScrollY(0);
  _state = State::RUNNING_TEXT;
  return true;
}
#endif

void InkEngine::applySettings(const AppSettings &settings) {
  g_marginPx = settings.marginPx;
  g_refreshInterval = settings.refreshInterval;
  _settings = settings;
  if (settings.choiceAnimationEnabled) {
    _cascadeOffsetMs = settings.choiceCascadeMs;
    _focusDelayMs = settings.choiceFocusDelayMs;
  } else {
    _cascadeOffsetMs = 0;
    _focusDelayMs = 0;
  }
  _displayManager.applySettings(settings);
}

const unsigned char *InkEngine::createSnapshot(std::size_t *outLength) {
  return _storyManager.createSnapshot(outLength);
}

void InkEngine::freeSnapshot() { _storyManager.freeSnapshot(); }

bool InkEngine::loadSnapshot(const unsigned char *data, std::size_t length) {
  if (_storyManager.loadSnapshot(data, length)) {
    _displayManager.clearHistory();
    _displayManager.setScrollY(0);
    _state = State::RUNNING_TEXT;
    return true;
  }
  return false;
}

void InkEngine::tickRunningText() {
  GfxRenderer *renderer = _display.getRenderer();
  int marginX = g_marginPx;
  int narrativeWidth = _display.getWidth() - (2 * marginX);
  int newLinesCount = 0;

  auto pushLine = [&](const std::string &str) {
    if (str.empty()) {
      _displayManager.addWrappedLine({TextBlock(), false, false, "", 0, true});
      newLinesCount++;
    } else if (renderer) {
      std::vector<TextRun> runs = InkRichTextParser::parse(str.c_str());
      auto wraps = renderer->wrapRichText(1, runs, narrativeWidth,
                                          100); // 1 = FONT_NARRATIVE
      for (size_t i = 0; i < wraps.size(); ++i) {
        bool eop = (i == wraps.size() - 1);
        _displayManager.addWrappedLine({wraps[i], false, false, "", 0, eop});
        newLinesCount++;
      }
    } else {
      TextBlock tb;
      tb.addRun(str, EpdFontFamily::REGULAR);
      _displayManager.addWrappedLine({tb, false, false, "", 0, true});
      newLinesCount++;
    }
  };

  auto &runner = _storyManager.runner();
  if (!runner)
    return;

  while (runner->can_continue()) {
    const char *line = runner->getline_alloc();
    if (line) {
      std::string s(line);
      if (!s.empty() && s.back() == '\n') {
        s.pop_back();
      }

      if (runner->has_tags()) {
        for (size_t i = 0; i < runner->num_tags(); i++) {
          const char *tag = runner->get_tag(i);
          if (tag && strncasecmp(tag, "IMAGE:", 6) == 0) {
            const char *pathStr = tag + 6;
            while (*pathStr == ' ')
              pathStr++;

            WrappedLine wl;
            wl.isImage = true;
            wl.imagePath = pathStr;
            wl.imageHeight = _storyManager.getImageHeight(pathStr);
            wl.endOfParagraph = true;

            _displayManager.addWrappedLine(wl);
            newLinesCount++;
          }
        }
      }

      if (s.empty() && runner->has_tags()) {
      } else {
        pushLine(s);
      }
    }
  }

#ifdef PLATFORM_ESP32
  static constexpr size_t kMaxHistoryLines = 100;
#else
  static constexpr size_t kMaxHistoryLines = 800;
#endif

  while (_displayManager.getHistorySize() > kMaxHistoryLines) {
    _displayManager.popOldestLine();
  }

  if (runner->has_choices()) {
    _displayManager.collectChoices(runner);
    _displayManager.doAutoScroll(newLinesCount, true);
    _choiceTurnStartMs = millis();
    _initialChoiceDelayMs =
        _settings.choiceAnimationEnabled
            ? std::min((uint32_t)4000, (uint32_t)(newLinesCount * 200))
            : 0;
    _revealStartMs = 0;
    _lastAnimFrameMs = 0;
    if (!_settings.choiceAnimationEnabled) {
      _displayManager.setRevealStarted(true);
      _displayManager.setRevealStep(_displayManager.getNumChoices() + 1);
    } else {
      _displayManager.setRevealStarted(false);
      _displayManager.setRevealStep(0);
    }
    _state = State::SHOWING_CHOICES;
    _needsRedraw = true;
  } else {
    _displayManager.setupStoryEndedChoices();
    _displayManager.doAutoScroll(newLinesCount, true);
    _choiceTurnStartMs = millis();
    _initialChoiceDelayMs =
        _settings.choiceAnimationEnabled
            ? std::min((uint32_t)4000, (uint32_t)(newLinesCount * 200))
            : 0;
    _revealStartMs = 0;
    _lastAnimFrameMs = 0;
    if (!_settings.choiceAnimationEnabled) {
      _displayManager.setRevealStarted(true);
      _displayManager.setRevealStep(_displayManager.getNumChoices() + 1);
    } else {
      _displayManager.setRevealStarted(false);
      _displayManager.setRevealStep(0);
    }
    _state = State::STORY_ENDED;
    _needsRedraw = true;
  }
}

void InkEngine::updateAnimation() {
  if (!_settings.choiceAnimationEnabled) {
    if (!_displayManager.getRevealStarted() ||
        _displayManager.getRevealStep() <= _displayManager.getNumChoices()) {
      _displayManager.setRevealStarted(true);
      _displayManager.setRevealStep(_displayManager.getNumChoices() + 1);
      _needsRedraw = true;
    }
    return;
  }

  uint32_t now = millis();
  if (!_displayManager.getRevealStarted()) {
    if ((now - _choiceTurnStartMs >= _initialChoiceDelayMs) &&
        _displayManager.isChoicesVisible()) {
      _displayManager.setRevealStarted(true);
      _revealStartMs = now;
      _lastAnimFrameMs = now;
      if (_cascadeOffsetMs == 0 && _focusDelayMs == 0) {
        _displayManager.setRevealStep(_displayManager.getNumChoices() + 1);
      } else if (_cascadeOffsetMs == 0) {
        _displayManager.setRevealStep(_displayManager.getNumChoices());
      } else {
        _displayManager.setRevealStep(1);
      }
      _needsRedraw = true;
    }
  } else if (_displayManager.getRevealStep() <=
             _displayManager.getNumChoices()) {
    uint32_t delay =
        (_displayManager.getRevealStep() == _displayManager.getNumChoices())
            ? _focusDelayMs
            : _cascadeOffsetMs;
    if (delay == 0) {
      if (_displayManager.getRevealStep() == _displayManager.getNumChoices()) {
        _displayManager.setRevealStep(_displayManager.getNumChoices() + 1);
      } else if (_cascadeOffsetMs == 0) {
        _displayManager.setRevealStep(_displayManager.getNumChoices());
      } else {
        _displayManager.setRevealStep(_displayManager.getRevealStep() + 1);
      }
      _lastAnimFrameMs = now;
      _needsRedraw = true;
    } else if (now - _lastAnimFrameMs >= delay) {
      _lastAnimFrameMs = now;
      _displayManager.setRevealStep(_displayManager.getRevealStep() + 1);
      _needsRedraw = true;
    }
  }
}

void InkEngine::update() {
  _needsRedraw = false;

  switch (_state) {
  case State::RUNNING_TEXT:
    tickRunningText();
    break;
  case State::SHOWING_CHOICES:
    _state = State::WAITING_INPUT;
    _needsRedraw = true;
    break;
  case State::WAITING_INPUT: {
    updateAnimation();
    _inputHandler.tickWaitingInput(*this, _displayManager, _storyManager,
                                   _display, _settings, _frontlight,
                                   _batteryWidget);
    break;
  }
  case State::STORY_ENDED: {
    updateAnimation();
    _inputHandler.tickStoryEnded(*this, _displayManager, _storyManager,
                                 _display, _settings, _frontlight,
                                 _batteryWidget);
    break;
  }
  case State::SAVE_STUB:
    _state = State::WAITING_INPUT;
    _needsRedraw = true;
    break;
  case State::DONE:
  case State::IDLE:
    break;
  }

  if (_needsRedraw) {
    _displayManager.redraw(_storyManager, _settings, _refreshCount);
  }

#ifdef PLATFORM_NATIVE
  SDL_Delay(16);
#else
  delay(16);
#endif
}
