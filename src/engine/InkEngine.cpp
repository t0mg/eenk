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
#elif defined(PLATFORM_ESP32)
#include <Arduino.h>
#include <esp_heap_caps.h>
#endif

extern int g_marginPx;
extern int g_refreshInterval;
int g_marginPx = 16;
int g_refreshInterval = 10;

InkEngine::InkEngine(IDisplay &display, IInput &input, IStorage &storage)
    : _display(display), _input(input), _storage(storage),
      _storyManager(storage), _displayManager(display), _inputHandler(input) {}

InkEngine::~InkEngine() {}

static uint32_t computeCrc32(const unsigned char *data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
  }
  return ~crc;
}

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

  char saveBuf[256] = {};
  StoryMetadata::getSavePath(path, saveBuf, sizeof(saveBuf));

  std::size_t rawSize = 0;
  const unsigned char *rawData = _storage.readFileBinary(path, &rawSize);
  uint32_t storyHash = 0;
  if (rawData && rawSize > 0) {
    storyHash = computeCrc32(rawData, rawSize);
    _storage.freeBuffer(rawData);
  }

  _saveManager.init(saveBuf, storyHash);
  if (_saveManager.loadSaveFile(_storage, &_storyManager) &&
      _saveManager.restoreMainProgress(_storyManager, _displayManager, &_storage)) {
    // Restored main progress or fallback checkpoint successfully
  } else {
    _displayManager.clearHistory();
    _displayManager.setScrollY(0);
  }

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

  if (storyPath && storyPath[0]) {
    char saveBuf[256] = {};
    StoryMetadata::getSavePath(storyPath, saveBuf, sizeof(saveBuf));
    uint32_t storyHash = computeCrc32(data, size);
    _saveManager.init(saveBuf, storyHash);
    if (_saveManager.loadSaveFile(_storage, &_storyManager) &&
        _saveManager.restoreMainProgress(_storyManager, _displayManager, &_storage)) {
      // Restored main progress or fallback checkpoint successfully
    } else {
      _displayManager.clearHistory();
      _displayManager.setScrollY(0);
    }
  } else {
    _displayManager.clearHistory();
    _displayManager.setScrollY(0);
  }

  _state = State::RUNNING_TEXT;
  return true;
}
#endif

void InkEngine::applySettings(const AppSettings &settings) {
  g_marginPx = settings.marginPx;
  g_refreshInterval = settings.refreshInterval;
  _settings = settings;
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

bool InkEngine::parseCheckpointTag(const char *rawTag, std::string &outTitle) {
  if (!rawTag)
    return false;
  while (*rawTag == ' ' || *rawTag == '\t')
    rawTag++;

  size_t prefixLen = 0;
  if (strncasecmp(rawTag, "CHECKPOINT", 10) == 0) {
    prefixLen = 10;
  } else if (strncasecmp(rawTag, "CHAPTER", 7) == 0) {
    prefixLen = 7;
  }

  if (prefixLen == 0)
    return false;

  const char *after = rawTag + prefixLen;
  while (*after == ' ' || *after == '\t')
    after++;
  if (*after == ':') {
    after++;
    while (*after == ' ' || *after == '\t')
      after++;
  }
  std::string t(after);
  while (!t.empty() &&
         (t.back() == ' ' || t.back() == '\t' || t.back() == '\r' ||
          t.back() == '\n')) {
    t.pop_back();
  }
  outTitle = t;
  return true;
}

bool InkEngine::choose(size_t index) {
  if (!_storyManager.runner() || index >= _storyManager.runner()->num_choices()) {
    return false;
  }
  bool choiceHasCp = false;
  std::string choiceCpTitle;
  const ink::runtime::choice *c = _storyManager.runner()->get_choice(index);
  if (c && c->has_tags()) {
    for (size_t ti = 0; ti < c->num_tags(); ++ti) {
      std::string t;
      if (parseCheckpointTag(c->get_tag(ti), t)) {
        choiceHasCp = true;
        choiceCpTitle = t;
        break;
      }
    }
  }
  _displayManager.markHistoryOld();
  _storyManager.runner()->choose(index);
  if (choiceHasCp) {
    triggerCheckpoint(choiceCpTitle);
  }
  _state = State::RUNNING_TEXT;
  return true;
}

void InkEngine::triggerCheckpoint(const std::string &checkpointTitle) {
#ifdef PLATFORM_ESP32
  // Ensure sufficient contiguous free heap block to safely create snapshot (14 KB)
  size_t largestFree = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (largestFree < 14336) {
    printf("[InkEngine] Low contiguous heap (%u bytes largest block, %u bytes free), skipping checkpoint snapshot\n",
           (unsigned)largestFree, (unsigned)ESP.getFreeHeap());
    return;
  }
#endif
  size_t snapLen = 0;
  const unsigned char *snap = _storyManager.createSnapshot(&snapLen);
  if (snap && snapLen > 0) {
    _saveManager.clearMainProgress();
    _saveManager.saveCheckpoint(checkpointTitle, snap, snapLen,
                                _displayManager.getHistory(), /*borrowSnapshot=*/true);
    if (_saveManager.writeSaveFile(_storage)) {
      printf("[InkEngine] Checkpoint saved to SD: '%s' (%u bytes snapshot)\n",
             checkpointTitle.c_str(), (unsigned)snapLen);
    } else {
      printf("[InkEngine] Failed to write checkpoint to SD: '%s'\n", checkpointTitle.c_str());
    }
    _storyManager.freeSnapshot();
  } else {
    _storyManager.freeSnapshot();
  }
}

void InkEngine::handleRuntimeError(const char *errorMsg) {
  SystemUI ui(_display);
  std::string msg = "A story error occurred:\n\n";
  msg += (errorMsg && errorMsg[0]) ? errorMsg : "Unknown runtime assert";

  bool hasCheckpoint = _saveManager.hasUnnamedCheckpoint() || _saveManager.hasNamedCheckpoints();
  if (hasCheckpoint) {
    msg += "\n\nRewind to last checkpoint?";
    bool rewind = ui.showConfirmDialog(_input, "Story Error", msg.c_str());
    if (rewind) {
      int cpIdx = _saveManager.getUnnamedCheckpointIndex();
      if (cpIdx < 0 && !_saveManager.getCheckpoints().empty()) {
        cpIdx = static_cast<int>(_saveManager.getCheckpoints().size()) - 1;
      }
      if (cpIdx >= 0 && _saveManager.restoreCheckpoint(cpIdx, _storyManager, _displayManager, &_storage)) {
        _saveManager.writeSaveFile(_storage);
        incrementRefreshCount();
        _state = State::RUNNING_TEXT;
        requestRedraw();
        return;
      }
    }
  } else {
    msg += "\n\nRestart story?";
    bool restart = ui.showConfirmDialog(_input, "Story Error", msg.c_str());
    if (restart) {
      _saveManager.clearAll(_storage);
      _displayManager.clearHistory();
      _displayManager.setScrollY(0);
      _storyManager.createFreshRunner();
      incrementRefreshCount();
      _state = State::RUNNING_TEXT;
      requestRedraw();
      return;
    }
  }

  // If user declined rewind/restart or restore failed, cleanly exit
  setShouldSleep(false);
  _state = State::DONE;
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

  while (true) {
    ink::g_ink_has_jmp_buf = true;
    if (setjmp(ink::g_ink_jmp_buf) != 0) {
      ink::g_ink_has_jmp_buf = false;
      handleRuntimeError(ink::g_ink_last_error);
      return;
    }
    bool canContinue = runner->can_continue();
    const char *line = canContinue ? runner->getline_alloc() : nullptr;
    ink::g_ink_has_jmp_buf = false;

    if (!canContinue) {
      break;
    }

    if (line) {
      std::string s(line);
      if (!s.empty() && s.back() == '\n') {
        s.pop_back();
      }
      size_t first = s.find_first_not_of(" \t\r\n");
      if (first == std::string::npos) {
        s.clear();
      } else {
        size_t last = s.find_last_not_of(" \t\r\n");
        s = s.substr(first, (last - first + 1));
      }

      bool hasImage = false;
      std::string imagePath = "";
      bool hasCheckpointTag = false;
      std::string checkpointTitle = "";

      auto processTag = [&](const char *rawTag) {
        if (!rawTag)
          return;
        while (*rawTag == ' ' || *rawTag == '\t')
          rawTag++;

        if (strncasecmp(rawTag, "IMAGE:", 6) == 0) {
          const char *pathStr = rawTag + 6;
          while (*pathStr == ' ' || *pathStr == '\t')
            pathStr++;
          hasImage = true;
          imagePath = pathStr;
        } else {
          std::string title;
          if (parseCheckpointTag(rawTag, title)) {
            hasCheckpointTag = true;
            checkpointTitle = title;
          }
        }
      };

      if (runner->has_tags()) {
        for (size_t i = 0; i < runner->num_tags(); i++) {
          processTag(runner->get_tag(i));
        }
      }
      if (runner->has_knot_tags()) {
        for (size_t i = 0; i < runner->num_knot_tags(); i++) {
          processTag(runner->get_knot_tag(i));
        }
      }

      if (hasImage) {
        WrappedLine wl;
        wl.isImage = true;
        wl.imagePath = imagePath;
        wl.imageHeight = _storyManager.getImageHeight(imagePath.c_str());
        wl.endOfParagraph = true;
        _displayManager.addWrappedLine(wl);
        newLinesCount++;
      } else if (!s.empty()) {
        pushLine(s);
      }

      if (hasCheckpointTag) {
        triggerCheckpoint(checkpointTitle);
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
    if (newLinesCount > 0) {
      _displayManager.setChoicesRevealed(false);
      _displayManager.doAutoScroll(newLinesCount, false);
    } else {
      _displayManager.setChoicesRevealed(true);
      _displayManager.doAutoScroll(0, true);
    }
    _state = State::WAITING_INPUT;
    _needsRedraw = true;
  } else {
    _displayManager.setupStoryEndedChoices();
    if (newLinesCount > 0) {
      _displayManager.setChoicesRevealed(false);
      _displayManager.doAutoScroll(newLinesCount, false);
    } else {
      _displayManager.setChoicesRevealed(true);
      _displayManager.doAutoScroll(0, true);
    }
    _state = State::STORY_ENDED;
    _needsRedraw = true;
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
    _inputHandler.tickWaitingInput(*this, _displayManager, _storyManager,
                                   _display, _settings, _frontlight,
                                   _batteryWidget);
    break;
  }
  case State::STORY_ENDED: {
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
