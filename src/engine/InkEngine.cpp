// Include <cmath> FIRST so GCC 15's specfun.h special-math functions are
// emitted at global scope rather than inside ink::runtime namespace.
#include <cmath>

#include "engine/InkEngine.h"

// InkCPP public API (already via InkEngine.h, but choice.h is also needed)
#include <choice.h>

#include "engine/InkEngine.h"
#include "GfxRenderer.h"
#include "InkRichTextParser.h"
#include "os/AppSettings.h"
#include "ui/SystemUI.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <snapshot.h>

#ifdef PLATFORM_NATIVE
#include <SDL.h> // for SDL_Delay
#endif

#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <StreamingEpdFont.h>
#include <StreamingEpdFontFamily.h>

// Builtin fonts — sans-serif (reader_* family)
// reader_medium_2b.h = 16pt (sans-medium, default)
// reader_2b.h        = 14pt (sans-small)
#include <builtinFonts/reader_2b.h>
#include <builtinFonts/reader_bold_2b.h>
#include <builtinFonts/reader_italic_2b.h>
#include <builtinFonts/reader_medium_2b.h>
#include <builtinFonts/reader_medium_italic_2b.h>
#include <builtinFonts/reader_medium_bold_2b.h>
// UI/choice fonts
#include <builtinFonts/ui_10.h>
#include <builtinFonts/ui_bold_10.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_12.h>
#include <builtinFonts/small14.h>
// Note: reader_xsmall_*.h and reader_large_*.h are NOT included in flash.
// Those sizes are served as .epdfont files from the SD card /fonts/ directory.

// Builtin font table (token → EpdFontData* mapping)
// Defined here because InkEngine includes all the font data headers.
//
// Index 0: sans-medium (16pt) — DEFAULT
// Index 1: sans-small  (14pt)
#include <BuiltinFonts.h>
extern const BuiltinFontEntry kBuiltinFonts[] = {
    // ── Sans-serif (Reader family) ───────────────────────────────────────────

    { "sans-medium",  "Sans M",   // index 0 — default story font
      &reader_medium_2b,         &reader_medium_bold_2b,  &reader_medium_italic_2b,  nullptr },

    { "sans-small",   "Sans S",   // index 1 — 14pt
      &reader_2b,                &reader_bold_2b,          &reader_italic_2b,          nullptr },

    // ── Serif (Literata) — compiled in only when EENK_HAS_LITERATA is set ────
#ifdef EENK_HAS_LITERATA
    { "serif-medium", "Serif M",
      &literata_medium_2b, &literata_medium_bold_2b, &literata_medium_italic_2b, nullptr },

    { "serif-large",  "Serif L",
      &literata_large_2b,  &literata_large_bold_2b,  &literata_large_italic_2b,  nullptr },
#endif

    // ── Short aliases (resolve to medium size) ────────────────────────────────
    { "sans",  "Sans",  &reader_medium_2b, &reader_medium_bold_2b, &reader_medium_italic_2b, nullptr },
#ifdef EENK_HAS_LITERATA
    { "serif", "Serif", &literata_medium_2b, &literata_medium_bold_2b, &literata_medium_italic_2b, nullptr },
#endif
};

extern const size_t kBuiltinFontCount =
    sizeof(kBuiltinFonts) / sizeof(kBuiltinFonts[0]);

#include "os/AppSettings.h"
#include "ui/StoryMetadata.h"

static constexpr int FONT_NARRATIVE = 1;
static constexpr int FONT_CHOICE    = 2;

// ── Choice font pool ─────────────────────────────────────────────────────────
// UI fonts used for the choice list. Index matches AppSettings::choiceFontIndex.
static const EpdFontData* const kChoiceFontData[] = {
    &ui_10,
    &ui_bold_10,
    &ui_12,
    &ui_bold_12,
    &small14,
};
static constexpr int kChoiceFontCount =
    static_cast<int>(sizeof(kChoiceFontData) / sizeof(kChoiceFontData[0]));

static int g_marginPx        = 16;
static int g_refreshInterval = 10;


// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

InkEngine::InkEngine(IDisplay &display, IInput &input, IStorage &storage)
    : _display(display), _input(input), _storage(storage) {}

InkEngine::~InkEngine() {
  freeSnapshot();
  if (_story) {
    delete _story;
    _story = nullptr;
  }
  if (_storyBuf) {
    _storage.freeBuffer(_storyBuf);
    _storyBuf = nullptr;
  }
  // Clean up any streaming SD font family.
  if (_streamingFamily) {
    delete _streamingFamily;
    _streamingFamily = nullptr;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// FontResolver — selects the active narrative EpdFontFamily for a story.
//
// Resolution order:
//   1. overrideStoryFont is set → use AppSettings::storyFontIndex (builtin)
//   2. hint is empty            → use AppSettings::storyFontIndex (builtin)
//   3. hint matches builtin token (case-insensitive) → use that builtin
//   4. otherwise                → SD card font family by stem name:
//       a. sidecar: /eenk/<storyBase>/<stem>[suffix].epdfont
//       b. shared:  /fonts/<stem>[suffix].epdfont
//       c. fallback to AppSettings::storyFontIndex if not found
// ─────────────────────────────────────────────────────────────────────────────

// Build EpdFontFamily from a BuiltinFontEntry using caller-supplied storage.
// The four EpdFont slots (r,b,it,bi) must outlive the returned family reference.
static void buildFamilyFromEntry(const BuiltinFontEntry* e,
                                  EpdFont& r, EpdFont& b, EpdFont& it, EpdFont& bi,
                                  EpdFontFamily& family) {
  const EpdFontData* reg = e->regular;
  r  = EpdFont(reg);
  b  = EpdFont(e->bold       ? e->bold       : reg);
  it = EpdFont(e->italic     ? e->italic     : reg);
  bi = EpdFont(e->boldItalic ? e->boldItalic : (e->bold ? e->bold : reg));
  family = EpdFontFamily(&r, &b, &it, &bi);
}

// ─────────────────────────────────────────────────────────────────────────────
#ifdef PLATFORM_NATIVE
bool InkEngine::loadStory(const char *path) {
  std::size_t size = 0;
  _storyBuf = _storage.readFileBinary(path, &size);
  if (!_storyBuf || size == 0) {
    fprintf(stderr, "[InkEngine] Failed to read: %s\n", path);
    return false;
  }

  const unsigned char* dataToLoad = _storyBuf;
  std::size_t sizeToLoad = size;

  // Parse EENK header if present.
  StoryMetadata meta;
  memset(&meta, 0, sizeof(meta));
  if (sizeToLoad >= 128 && StoryMetadata::hasHeader(_storyBuf, sizeToLoad)) {
    StoryMetadata::parse(_storyBuf, sizeToLoad, &meta);
    dataToLoad += 128;
    sizeToLoad -= 128;
  }

  _story = ink::runtime::story::from_binary(dataToLoad, sizeToLoad, false);
  if (!_story) {
    fprintf(stderr, "[InkEngine] story::from_binary() failed\n");
    _storage.freeBuffer(_storyBuf);
    _storyBuf = nullptr;
    return false;
  }

  _globals = _story->new_globals();
  _runner  = _story->new_runner(_globals);

  printf("[InkEngine] Story loaded — %zu bytes\n", size);

  // Derive story base name from path for sidecar font lookup.
  char storyBase[64] = {};
  const char* lastSlash = strrchr(path, '/');
  const char* fname = lastSlash ? lastSlash + 1 : path;
  strncpy(storyBase, fname, sizeof(storyBase) - 1);
  // strip .bin extension
  char* dot = strrchr(storyBase, '.');
  if (dot) *dot = '\0';

  _resolveAndApplyFont(meta, storyBase);

  _wrappedLines.clear();
  _scrollY    = 0;
  _maxScrollY = 0;
  _state      = State::RUNNING_TEXT;
  return true;
}
#endif

void InkEngine::applySettings(const AppSettings& settings) {
  g_marginPx        = settings.marginPx;
  g_refreshInterval = settings.refreshInterval;
  _settings         = settings;

  // Wire choice font to the renderer (narrative font is resolved at story load
  // time via FontResolver; choice font always follows the user setting).
  GfxRenderer* renderer = _display.getRenderer();
  if (renderer) {
    int idx = settings.choiceFontIndex;
    if (idx < 0 || idx >= kChoiceFontCount) idx = 2; // default ui_12
    static EpdFont        choiceFont(kChoiceFontData[idx]);
    static EpdFontFamily  choiceFamily(&choiceFont);
    choiceFont   = EpdFont(kChoiceFontData[idx]);
    choiceFamily = EpdFontFamily(&choiceFont);
    renderer->insertFont(FONT_CHOICE, choiceFamily);
  }
}

#ifndef PLATFORM_NATIVE
bool InkEngine::loadStory(const unsigned char *data,
                          std::size_t size) {
  // NOTE: _storyBuf is NOT set — we don't own this memory (it's mmap'd)

  const unsigned char* dataToLoad = data;
  std::size_t sizeToLoad = size;

  // Parse EENK header if present.
  StoryMetadata meta;
  memset(&meta, 0, sizeof(meta));
  if (sizeToLoad >= 128 && StoryMetadata::hasHeader(dataToLoad, sizeToLoad)) {
    StoryMetadata::parse(dataToLoad, sizeToLoad, &meta);
    dataToLoad += 128;
    sizeToLoad -= 128;
  }

  _story = ink::runtime::story::from_binary(dataToLoad, sizeToLoad, false);
  if (!_story) {
    fprintf(stderr, "[InkEngine] story::from_binary() failed (mmap)\n");
    return false;
  }

  _globals = _story->new_globals();
  _runner  = _story->new_runner(_globals);

  printf("[InkEngine] Story loaded from memory — %zu bytes\n", size);

  // On ESP32 the story path is not available here; storyBase is left empty
  // so sidecar lookup is skipped and only /fonts/ is tried.
  _resolveAndApplyFont(meta, "");

  _wrappedLines.clear();
  _scrollY    = 0;
  _maxScrollY = 0;
  _state      = State::RUNNING_TEXT;
  return true;
}
#endif

const unsigned char *InkEngine::createSnapshot(std::size_t *outLength) {
  freeSnapshot();
  if (!_runner)
    return nullptr;

  _currentSnapshot = _runner->create_snapshot();
  if (_currentSnapshot) {
    *outLength = _currentSnapshot->get_data_len();
    return _currentSnapshot->get_data();
  }
  return nullptr;
}

void InkEngine::freeSnapshot() {
  if (_currentSnapshot) {
    delete _currentSnapshot;
    _currentSnapshot = nullptr;
  }
}

bool InkEngine::loadSnapshot(const unsigned char *data, std::size_t length) {
  if (!_story)
    return false;

  // from_binary takes ownership of data if freeOnDestroy is true.
  // We pass false so we can manage the memory buffer ourselves (e.g. from SD
  // read).
  ink::runtime::snapshot *snap =
      ink::runtime::snapshot::from_binary(data, length, false);
  if (!snap)
    return false;

  _globals = _story->new_globals_from_snapshot(*snap);
  _runner = _story->new_runner_from_snapshot(*snap, _globals);

  delete snap;

  _wrappedLines.clear();
  _scrollY = 0;
  _maxScrollY = 0;
  _state = State::RUNNING_TEXT;

  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FontResolver implementation
// ─────────────────────────────────────────────────────────────────────────────

void InkEngine::_resolveAndApplyFont(const StoryMetadata& meta, const char* storyBase) {
  // Clean up any previously active SD font family.
  if (_streamingFamily) {
    delete _streamingFamily;
    _streamingFamily = nullptr;
  }

  GfxRenderer* renderer = _display.getRenderer();

  // Static storage for the active narrative builtin font family.
  // Initialised to reader_medium_2b so they are never truly "empty".
  static EpdFont       s_r(&reader_medium_2b),  s_b(&reader_medium_2b),
                       s_it(&reader_medium_2b), s_bi(&reader_medium_2b);
  static EpdFontFamily s_narrFamily(&s_r, &s_b, &s_it, &s_bi);

  // ── Helper: apply a builtin font entry ──────────────────────────────────
  auto applyBuiltin = [&](size_t index) {
    const BuiltinFontEntry* e = getBuiltinByIndex(index);
    buildFamilyFromEntry(e, s_r, s_b, s_it, s_bi, s_narrFamily);
    if (renderer) {
      renderer->removeStreamingFont(FONT_NARRATIVE);
      renderer->insertFont(FONT_NARRATIVE, s_narrFamily);
    }
    printf("[InkEngine] Font: builtin '%s'\n", e->token);
  };

  // ── Build SD dirs ───────────────────────────────────────────────────────
  char sidecarDir[128] = {};
  if (storyBase && storyBase[0]) {
    snprintf(sidecarDir, sizeof(sidecarDir), "/eenk/%s", storyBase);
  }

  const char* dirs[3];
  int ndirs = 0;
  if (sidecarDir[0]) dirs[ndirs++] = sidecarDir;
  dirs[ndirs++] = "/fonts";
  dirs[ndirs]   = nullptr;

  auto applySdFont = [&](const char* stemName) {
    _streamingFamily = new StreamingEpdFontFamily();
    if (_streamingFamily->load(stemName, dirs)) {
      if (renderer) {
        static EpdFont       s_sdR(&ui_12);
        static EpdFont       s_sdB(&ui_12);
        static EpdFont       s_sdI(&ui_12);
        static EpdFont       s_sdBI(&ui_12);
        static EpdFontFamily s_sdFamily(&s_sdR);
        s_sdR = EpdFont(_streamingFamily->getData(EpdFontFamily::REGULAR));
        s_sdB = EpdFont(_streamingFamily->getData(EpdFontFamily::BOLD));
        s_sdI = EpdFont(_streamingFamily->getData(EpdFontFamily::ITALIC));
        s_sdBI = EpdFont(_streamingFamily->getData(EpdFontFamily::BOLD_ITALIC));
        s_sdFamily = EpdFontFamily(&s_sdR, &s_sdB, &s_sdI, &s_sdBI);
        renderer->insertFont(FONT_NARRATIVE, s_sdFamily);

        renderer->removeStreamingFont(FONT_NARRATIVE);
        for (int i = 0; i < 4; ++i) {
          auto st = static_cast<EpdFontFamily::Style>(i);
          if (_streamingFamily->slot(st)) {
            renderer->setStreamingFont(FONT_NARRATIVE, st, _streamingFamily->slot(st));
          }
        }
      }
      printf("[InkEngine] Font: SD family '%s'\n", stemName);
      return true;
    }
    delete _streamingFamily;
    _streamingFamily = nullptr;
    return false;
  };

  auto applyUserSetting = [&]() {
    const char* fontName = _settings.storyFont;
    
    const BuiltinFontEntry* e = findBuiltinByToken(fontName);
    if (e) {
      applyBuiltin(e - kBuiltinFonts);
      return;
    }
    
    if (applySdFont(fontName)) {
      return;
    }

    printf("[InkEngine] Font: Setting '%s' failed, falling back to default\n", fontName);
    applyBuiltin(0);
  };

  // ── 1. Override or no hint → user setting ─────────────────────────────
  if (_settings.overrideStoryFont || meta.fontNameLen == 0) {
    applyUserSetting();
    return;
  }

  char stem[16] = {};
  meta.getFontStem(stem, sizeof(stem));
  if (!stem[0]) {
    applyUserSetting();
    return;
  }

  // ── 2. Builtin token lookup (case-insensitive) ─────────────────────────
  const BuiltinFontEntry* e = findBuiltinByToken(stem);
  if (e) {
    applyBuiltin(e - kBuiltinFonts);
    return;
  }

  // ── 3. SD card font family ─────────────────────────────────────────────
  if (applySdFont(stem)) {
    return;
  }

  // ── 4. Fallback to user setting ────────────────────────────────────────
  printf("[InkEngine] Font: SD family '%s' not found, falling back to user setting\n", stem);
  applyUserSetting();
}


// ─────────────────────────────────────────────────────────────────────────────
void InkEngine::update() {
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
void InkEngine::tickRunningText() {
  GfxRenderer *renderer = _display.getRenderer();
  int marginX = g_marginPx;
  int narrativeWidth = _display.getWidth() - (2 * marginX);
  int newLinesCount = 0;

  auto pushLine = [&](const std::string &str) {
    if (str.empty()) {
      _wrappedLines.push_back({TextBlock(), false});
      newLinesCount++;
    } else if (renderer) {
      std::vector<TextRun> runs = InkRichTextParser::parse(str.c_str());
      auto wraps = renderer->wrapRichText(FONT_NARRATIVE, runs, narrativeWidth, 100);
      for (auto &w : wraps) {
        _wrappedLines.push_back({w, false});
        newLinesCount++;
      }
    } else {
      TextBlock tb;
      tb.addRun(str, EpdFontFamily::REGULAR);
      _wrappedLines.push_back({tb, false});
      newLinesCount++;
    }
  };

  while (_runner->can_continue()) {
    const char *line = _runner->getline_alloc();
    if (line) {
      std::string s(line);
      while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
      }
      pushLine(s);
    }
  }

  while (_wrappedLines.size() > 800) {
    _wrappedLines.pop_front();
  }

  auto doAutoScroll = [&]() {
    if (!renderer)
      return;
    int height = _display.getHeight();
    int marginY = 24;
    int choiceHeight = getChoicesHeight(renderer);
    int documentHeight =
        _wrappedLines.size() * renderer->getLineHeight(FONT_NARRATIVE) +
        choiceHeight;
    int availableHeight = height - (2 * marginY);

    _maxScrollY = std::max(0, documentHeight - availableHeight);

    int newContentHeight =
        newLinesCount * renderer->getLineHeight(FONT_NARRATIVE) + choiceHeight;
    if (newContentHeight > availableHeight) {
      // Scroll so the first new line is at the top of the visible narrative
      // area
      int oldLinesCount =
          std::max(0, (int)_wrappedLines.size() - newLinesCount);
      _scrollY = std::min(
          _maxScrollY, oldLinesCount * renderer->getLineHeight(FONT_NARRATIVE));
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
void InkEngine::collectChoices() {
  _numChoices = 0;
  _selectedChoice = 0;

  GfxRenderer *renderer = _display.getRenderer();
  int narrativeWidth = 0;
  if (renderer) {
    int width = _display.getWidth();
    int marginX = g_marginPx;
    narrativeWidth = width - (2 * marginX);
  }

  for (const auto *c = _runner->begin(); c != _runner->end(); ++c) {
    if (_numChoices >= MAX_CHOICES)
      break;
    const char *txt = c->text();
    if (txt) {
      strncpy(_choiceText[_numChoices], txt, sizeof(_choiceText[0]) - 1);
      _choiceText[_numChoices][sizeof(_choiceText[0]) - 1] = '\0';
    } else {
      _choiceText[_numChoices][0] = '\0';
    }

    _wrappedChoices[_numChoices].clear();
    if (renderer && narrativeWidth > 0) {
      int indicatorWidth = 24;
      std::vector<TextRun> runs = InkRichTextParser::parse(_choiceText[_numChoices]);
      _wrappedChoices[_numChoices] = renderer->wrapRichText(
          FONT_CHOICE, runs, narrativeWidth - indicatorWidth, 100);
      if (_wrappedChoices[_numChoices].empty()) {
        TextBlock empty;
        empty.addRun("", EpdFontFamily::REGULAR);
        _wrappedChoices[_numChoices].push_back(empty);
      }
    }

    ++_numChoices;
  }
}

int InkEngine::getChoicesHeight(GfxRenderer *renderer) const {
  if (!renderer || _numChoices == 0)
    return 0;
  int lines = 0;
  for (int i = 0; i < _numChoices; ++i) {
    lines += std::max((size_t)1, _wrappedChoices[i].size());
  }
  int marginY = g_marginPx;
  int choiceLineHeight = renderer->getLineHeight(FONT_CHOICE);
  int choicePadding = choiceLineHeight / 3;
  return (lines * choiceLineHeight) + marginY +
         ((_numChoices - 1) * choicePadding);
}

// ─────────────────────────────────────────────────────────────────────────────
void InkEngine::tickWaitingInput() {
  ButtonEvent ev = _input.pollInput();

  bool choicesVisible = false;
  GfxRenderer *renderer = _display.getRenderer();
  if (renderer && _numChoices > 0) {
    int marginY = g_marginPx;
    int choiceHeight = getChoicesHeight(renderer);
    if (_scrollY > _maxScrollY - choiceHeight - marginY) {
      choicesVisible = true;
    }
  } else if (!renderer) {
    choicesVisible = true;
  }

  if (!choicesVisible) {
    if (ev == ButtonEvent::UP)
      ev = ButtonEvent::LEFT;
    if (ev == ButtonEvent::DOWN)
      ev = ButtonEvent::RIGHT;
  }

  switch (ev) {
  case ButtonEvent::UP:
    if (_selectedChoice > 0) {
      --_selectedChoice;
      redraw();
    } else {
      int scrollAmount = _display.getHeight() / 4;
      _scrollY -= scrollAmount;
      if (_scrollY < 0)
        _scrollY = 0;
      redraw();
    }
    break;
  case ButtonEvent::DOWN:
    if (_selectedChoice < _numChoices - 1)
      ++_selectedChoice;
    else if (_numChoices > 0)
      _selectedChoice = 0;
    redraw();
    break;
  case ButtonEvent::LEFT: {
    int scrollAmount = _display.getHeight() / 4;
    _scrollY -= scrollAmount;
    if (_scrollY < 0)
      _scrollY = 0;
    redraw();
    break;
  }
  case ButtonEvent::RIGHT: {
    int scrollAmount = _display.getHeight() / 4;
    _scrollY += scrollAmount;

    GfxRenderer *renderer = _display.getRenderer();
    if (renderer && _numChoices > 0) {
      int marginY = g_marginPx;
      int choiceHeight = getChoicesHeight(renderer);
      if (_scrollY > _maxScrollY - choiceHeight - marginY) {
        _scrollY = _maxScrollY;
      }
    }

    if (_scrollY > _maxScrollY)
      _scrollY = _maxScrollY;
    redraw();
    break;
  }
  case ButtonEvent::CONFIRM:
    if (!choicesVisible) {
      int scrollAmount = _display.getHeight() / 4;
      _scrollY += scrollAmount;

      GfxRenderer *renderer = _display.getRenderer();
      if (renderer && _numChoices > 0) {
        int marginY = g_marginPx;
        int choiceHeight = getChoicesHeight(renderer);
        if (_scrollY > _maxScrollY - choiceHeight - marginY) {
          _scrollY = _maxScrollY;
        }
      }

      if (_scrollY > _maxScrollY)
        _scrollY = _maxScrollY;
      redraw();
    } else if (_numChoices > 0) {
      for (auto &l : _wrappedLines)
        l.isOld = true;
      _runner->choose(static_cast<std::size_t>(_selectedChoice));
      _state = State::RUNNING_TEXT;
    }
    break;
  case ButtonEvent::BACK:
  case ButtonEvent::QUIT: {
    SystemUI ui(_display);
    if (ui.showConfirmDialog(_input, "Exit Story", "Return to the menu?")) {
      // Exit to menu: save state, then signal done so main loop reboots to MENU.
      _shouldSleep = false;
      _state = State::DONE;
    } else {
      redraw();
    }
    break;
  }
  case ButtonEvent::SLEEP:
    _shouldSleep = true;
    break;
  default:
    break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void InkEngine::tickStoryEnded() {
  ButtonEvent ev = _input.pollInput();
  if (ev == ButtonEvent::CONFIRM) {
    _globals = _story->new_globals();
    _runner = _story->new_runner(_globals);
    _wrappedLines.clear();
    _scrollY = 0;
    _maxScrollY = 0;
    _state = State::RUNNING_TEXT;
  } else if (ev == ButtonEvent::LEFT) {
    int scrollAmount = _display.getHeight() / 4;
    _scrollY -= scrollAmount;
    if (_scrollY < 0)
      _scrollY = 0;
    redraw();
  } else if (ev == ButtonEvent::RIGHT) {
    int scrollAmount = _display.getHeight() / 4;
    _scrollY += scrollAmount;

    GfxRenderer *renderer = _display.getRenderer();
    if (renderer && _numChoices > 0) {
      int marginY = g_marginPx;
      int choiceHeight = getChoicesHeight(renderer);
      if (_scrollY > _maxScrollY - choiceHeight - marginY) {
        _scrollY = _maxScrollY;
      }
    }

    if (_scrollY > _maxScrollY)
      _scrollY = _maxScrollY;
    redraw();
  } else if (ev == ButtonEvent::QUIT) {
    _state = State::DONE;
  } else if (ev == ButtonEvent::SLEEP) {
    _shouldSleep = true;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void InkEngine::redraw() {
  _display.clear();

  GfxRenderer *renderer = _display.getRenderer();
  if (!renderer) {
    // ── Text-mode (e.g. EspSerialDisplay) ───────────────────────────────
    for (const auto &line : _wrappedLines) {
      std::string s;
      for (const auto& r : line.block.runs) s += r.text;
      _display.drawNarrativeLine(s.c_str());
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

  int width = _display.getWidth();
  int height = _display.getHeight();

  int marginX = g_marginPx;
  int marginY = g_marginPx;
  int narrativeWidth = width - (2 * marginX);

  int lineHeight = renderer->getLineHeight(FONT_NARRATIVE);
  int choiceLineHeight = renderer->getLineHeight(FONT_CHOICE);

  int y = marginY - _scrollY;

  for (const auto &w : _wrappedLines) {
    if (!w.block.isEmpty() && (y + lineHeight > 0) && (y < height)) {
      renderer->setHalftone(w.isOld);
      renderer->drawRichText(FONT_NARRATIVE, marginX, y, w.block);
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

    int choicePadding = choiceLineHeight / 3;
    int indicatorWidth = 24;

    for (int i = 0; i < _numChoices; ++i) {
      if (i > 0) {
        // int lineY = y + (choicePadding / 2);
        // if (lineY > 0 && lineY < height) {
        //   renderer->setHalftone(true);
        //   renderer->fillRect(marginX, lineY, narrativeWidth, 1, true);
        //   renderer->setHalftone(false);
        // }
        y += choicePadding;
      }

      bool selected = (i == _selectedChoice);
      int choiceBlockLines = std::max((size_t)1, _wrappedChoices[i].size());
      int choiceBlockHeight = choiceBlockLines * choiceLineHeight;

      if ((y + choiceBlockHeight > 0) && (y < height)) {
        if (selected) {
          renderer->fillRect(marginX - 4, y, narrativeWidth + 8,
                             choiceBlockHeight, true);
        }
      }

      if (_wrappedChoices[i].empty()) {
        if (selected) {
          renderer->drawText(FONT_CHOICE, marginX, y, ">", !selected);
        }
        y += choiceLineHeight;
      } else {
        for (size_t l = 0; l < _wrappedChoices[i].size(); ++l) {
          if ((y + choiceLineHeight > 0) && (y < height)) {
            if (l == 0 && selected) {
              renderer->drawText(FONT_CHOICE, marginX, y, ">", !selected);
            }
            renderer->drawRichText(FONT_CHOICE, marginX + indicatorWidth, y,
                               _wrappedChoices[i][l], !selected);
          }
          y += choiceLineHeight;
        }
      }
    }
  }

  _display.present();
}
