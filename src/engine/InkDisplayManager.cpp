#include "InkDisplayManager.h"
#include "InkRichTextParser.h"
#include "InkStoryManager.h"
#include "ui/ImageWidget.h"
#include <BuiltinFonts.h>
#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <StreamingEpdFont.h>
#include <StreamingEpdFontFamily.h>
#include <choice.h>

static constexpr int FONT_NARRATIVE = 1;
static constexpr int FONT_CHOICE = 2;

// Builtin fonts
#include <builtinFonts/reader_2b.h>
#include <builtinFonts/reader_bold_2b.h>
#include <builtinFonts/reader_italic_2b.h>
#include <builtinFonts/reader_medium_2b.h>
#include <builtinFonts/reader_medium_bold_2b.h>
#include <builtinFonts/reader_medium_italic_2b.h>
#include <builtinFonts/small14.h>
#include <builtinFonts/ui_10.h>
#include <builtinFonts/ui_12.h>
#include <builtinFonts/ui_bold_10.h>
#include <builtinFonts/ui_bold_12.h>

extern const BuiltinFontEntry kBuiltinFonts[] = {
    {"sans-medium", "Sans M", &reader_medium_2b, &reader_medium_bold_2b,
     &reader_medium_italic_2b, nullptr},
    {"sans-small", "Sans S", &reader_2b, &reader_bold_2b, &reader_italic_2b,
     nullptr},
#ifdef eenk_HAS_LITERATA
    {"serif-medium", "Serif M", &literata_medium_2b, &literata_medium_bold_2b,
     &literata_medium_italic_2b, nullptr},
    {"serif-large", "Serif L", &literata_large_2b, &literata_large_bold_2b,
     &literata_large_italic_2b, nullptr},
#endif
    {"sans", "Sans", &reader_medium_2b, &reader_medium_bold_2b,
     &reader_medium_italic_2b, nullptr},
#ifdef eenk_HAS_LITERATA
    {"serif", "Serif", &literata_medium_2b, &literata_medium_bold_2b,
     &literata_medium_italic_2b, nullptr},
#endif
};

extern const size_t kBuiltinFontCount =
    sizeof(kBuiltinFonts) / sizeof(kBuiltinFonts[0]);

static const EpdFontData *const kChoiceFontData[] = {
    &ui_10, &ui_bold_10, &ui_12, &ui_bold_12, &small14,
};
static constexpr int kChoiceFontCount =
    static_cast<int>(sizeof(kChoiceFontData) / sizeof(kChoiceFontData[0]));

int WrappedLine::getHeight(GfxRenderer *renderer) const {
  if (!renderer)
    return 0;
  int h = isImage ? imageHeight : renderer->getLineHeight(FONT_NARRATIVE);
  if (endOfParagraph) {
    h += (renderer->getLineHeight(FONT_NARRATIVE) / 2);
  }
  return h;
}

InkDisplayManager::InkDisplayManager(IDisplay &display) : _display(display) {}

InkDisplayManager::~InkDisplayManager() {
  if (_streamingFamily) {
    delete _streamingFamily;
    _streamingFamily = nullptr;
  }
}

void InkDisplayManager::applySettings(const AppSettings &settings) {
  _settings = settings;
  GfxRenderer *renderer = _display.getRenderer();
  if (renderer) {
    int idx = settings.choiceFontIndex;
    if (idx < 0 || idx >= kChoiceFontCount)
      idx = 2; // default ui_12
    static EpdFont choiceFont(kChoiceFontData[idx]);
    static EpdFontFamily choiceFamily(&choiceFont);
    choiceFont = EpdFont(kChoiceFontData[idx]);
    choiceFamily = EpdFontFamily(&choiceFont);
    renderer->insertFont(FONT_CHOICE, choiceFamily);
  }
}

static void buildFamilyFromEntry(const BuiltinFontEntry *e, EpdFont &r,
                                 EpdFont &b, EpdFont &it, EpdFont &bi,
                                 EpdFontFamily &family) {
  const EpdFontData *reg = e->regular;
  r = EpdFont(reg);
  b = EpdFont(e->bold ? e->bold : reg);
  it = EpdFont(e->italic ? e->italic : reg);
  bi = EpdFont(e->boldItalic ? e->boldItalic : (e->bold ? e->bold : reg));
  family = EpdFontFamily(&r, &b, &it, &bi);
}

void InkDisplayManager::resolveAndApplyFont(const StoryMetadata &meta,
                                            const char *storyBase,
                                            const char *storyDir) {
  if (_streamingFamily) {
    delete _streamingFamily;
    _streamingFamily = nullptr;
  }
  GfxRenderer *renderer = _display.getRenderer();
  static EpdFont s_r(&reader_medium_2b), s_b(&reader_medium_2b),
      s_it(&reader_medium_2b), s_bi(&reader_medium_2b);
  static EpdFontFamily s_narrFamily(&s_r, &s_b, &s_it, &s_bi);

  auto applyBuiltin = [&](size_t index) {
    const BuiltinFontEntry *e = getBuiltinByIndex(index);
    buildFamilyFromEntry(e, s_r, s_b, s_it, s_bi, s_narrFamily);
    if (renderer) {
      renderer->removeStreamingFont(FONT_NARRATIVE);
      renderer->insertFont(FONT_NARRATIVE, s_narrFamily);
    }
    printf("[InkEngine] Font: builtin '%s'\n", e->token);
  };

  char sidecarDir1[512] = {};
  char sidecarDir2[512] = {};
  if (storyDir && storyDir[0]) {
    snprintf(sidecarDir1, sizeof(sidecarDir1), "%s", storyDir);
  }
  if (storyBase && storyBase[0]) {
#ifdef PLATFORM_NATIVE
    snprintf(sidecarDir2, sizeof(sidecarDir2), "stories/%s", storyBase);
#else
    snprintf(sidecarDir2, sizeof(sidecarDir2), "/stories/%s", storyBase);
#endif
  }

  const char *dirs[4];
  int ndirs = 0;
  if (sidecarDir1[0])
    dirs[ndirs++] = sidecarDir1;
  if (sidecarDir2[0] && strcmp(sidecarDir2, sidecarDir1) != 0)
    dirs[ndirs++] = sidecarDir2;
  dirs[ndirs++] = "/fonts";
  dirs[ndirs] = nullptr;

  auto applySdFont = [&](const char *stemName) {
    if (_streamingFamily) {
      delete _streamingFamily;
      _streamingFamily = nullptr;
    }
    _streamingFamily = new StreamingEpdFontFamily();
    if (_streamingFamily->load(stemName, dirs)) {
      if (renderer) {
        static EpdFont s_sdR(&ui_12), s_sdB(&ui_12), s_sdI(&ui_12),
            s_sdBI(&ui_12);
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
            renderer->setStreamingFont(FONT_NARRATIVE, st,
                                       _streamingFamily->slot(st));
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
    const char *fontName = _settings.storyFont;
    const BuiltinFontEntry *e = findBuiltinByToken(fontName);
    if (e) {
      applyBuiltin(e - kBuiltinFonts);
      return;
    }
    if (applySdFont(fontName)) {
      return;
    }
    printf("[InkEngine] Font: Setting '%s' failed, falling back to default\n",
           fontName);
    applyBuiltin(0);
  };

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

  const BuiltinFontEntry *e = findBuiltinByToken(stem);
  if (e) {
    applyBuiltin(e - kBuiltinFonts);
    return;
  }

  if (applySdFont(stem)) {
    return;
  }

  printf("[InkEngine] Font: SD family '%s' not found, falling back to user "
         "setting\n",
         stem);
  applyUserSetting();
}

void InkDisplayManager::clearHistory() { _wrappedLines.clear(); }

void InkDisplayManager::addWrappedLine(const WrappedLine &line) {
  _wrappedLines.push_back(line);
}

void InkDisplayManager::popOldestLine() {
  if (!_wrappedLines.empty()) {
    _wrappedLines.pop_front();
  }
}

void InkDisplayManager::markHistoryOld() {
  for (auto &line : _wrappedLines) {
    line.isOld = true;
  }
}

void InkDisplayManager::collectChoices(ink::runtime::runner &runner) {
  _numChoices = 0;
  _selectedChoice = 0;

  GfxRenderer *renderer = _display.getRenderer();
  int narrativeWidth = 0;
  if (renderer) {
    int width = _display.getWidth();
    int marginX = _settings.marginPx;
    narrativeWidth = width - (2 * marginX);
  }

  for (const auto *c = runner->begin(); c != runner->end(); ++c) {
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
      std::vector<TextRun> runs =
          InkRichTextParser::parse(_choiceText[_numChoices]);
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

void InkDisplayManager::setupStoryEndedChoices() {
  _numChoices = 2;
  _selectedChoice = 0;
  strncpy(_choiceText[0], "Exit to menu", sizeof(_choiceText[0]) - 1);
  strncpy(_choiceText[1], "Replay story", sizeof(_choiceText[1]) - 1);

  GfxRenderer *renderer = _display.getRenderer();
  if (renderer) {
    int width = _display.getWidth();
    int marginX = _settings.marginPx;
    int narrativeWidth = width - (2 * marginX);
    int indicatorWidth = 24;
    if (narrativeWidth > indicatorWidth) {
      int wrapWidth = narrativeWidth - indicatorWidth;
      _wrappedChoices[0] = renderer->wrapRichText(
          FONT_CHOICE, {TextRun(_choiceText[0], EpdFontFamily::Style::REGULAR)},
          wrapWidth, 2);
      _wrappedChoices[1] = renderer->wrapRichText(
          FONT_CHOICE, {TextRun(_choiceText[1], EpdFontFamily::Style::REGULAR)},
          wrapWidth, 2);
    }
  }
}
InkDisplayManager::ChoiceLayout InkDisplayManager::getChoiceLayout(GfxRenderer *renderer) const {
  ChoiceLayout layout;
  layout.marginY = _settings.marginPx;
  layout.choiceLineHeight = renderer->getLineHeight(FONT_CHOICE);
  layout.choicePadding = layout.choiceLineHeight / 3;
  layout.minChoiceHeight = std::max(
      layout.choiceLineHeight, _settings.touchChoicesEnabled ? 64 : layout.choiceLineHeight);
  return layout;
}

int InkDisplayManager::getChoicesHeight() const {
  GfxRenderer *renderer = _display.getRenderer();
  if (!renderer || _numChoices == 0)
    return 0;

  int totalHeight = 0;
  ChoiceLayout l = getChoiceLayout(renderer);

  for (int i = 0; i < _numChoices; ++i) {
    int lines = std::max((size_t)1, _wrappedChoices[i].size());
    int textHeight = lines * l.choiceLineHeight;
    int choiceBlockHeight = std::max(l.minChoiceHeight, textHeight);
    totalHeight += choiceBlockHeight;
  }

  return totalHeight + l.marginY + ((_numChoices - 1) * l.choicePadding);
}

void InkDisplayManager::doAutoScroll(int newLinesCount, bool showChoices) {
  GfxRenderer *renderer = _display.getRenderer();
  if (!renderer)
    return;

  int height = _display.getHeight();
  int marginY = 24;
  int choiceHeight = showChoices ? getChoicesHeight() : 0;

  int documentHeight = choiceHeight;
  for (const auto &w : _wrappedLines) {
    documentHeight += w.getHeight(renderer);
  }

  int availableHeight = height - (2 * marginY);
  _maxScrollY = std::max(0, documentHeight - availableHeight);

  int newContentHeight = choiceHeight;
  int startIndex = std::max(0, (int)_wrappedLines.size() - newLinesCount);
  for (size_t i = startIndex; i < _wrappedLines.size(); ++i) {
    const auto &w = _wrappedLines[i];
    newContentHeight += w.getHeight(renderer);
  }

  if (newContentHeight > availableHeight) {
    // Scroll so the first new line is at the top of the visible narrative area
    int oldLinesHeight = 0;
    for (int i = 0; i < startIndex; ++i) {
      const auto &w = _wrappedLines[i];
      oldLinesHeight += w.getHeight(renderer);
    }
    _scrollY = std::min(_maxScrollY, oldLinesHeight);
  } else {
    // Snap to bottom
    _scrollY = _maxScrollY;
  }
}

void InkDisplayManager::setScrollToBottom() { _scrollY = _maxScrollY; }

void InkDisplayManager::scrollToSelectedChoice() {
  GfxRenderer *renderer = _display.getRenderer();
  if (!renderer || _numChoices == 0) return;
  
  ChoiceLayout l = getChoiceLayout(renderer);
  
  int docY = l.marginY - _scrollY;
  for (const auto &w : _wrappedLines) { docY += w.getHeight(renderer); }
  docY += (l.marginY / 2) + 2 + (l.marginY / 2);
  
  for (int i = 0; i < _selectedChoice; ++i) {
     if (i > 0) docY += l.choicePadding;
     int lines = std::max((size_t)1, _wrappedChoices[i].size());
     int textHeight = lines * l.choiceLineHeight;
     docY += std::max(l.minChoiceHeight, textHeight);
  }
  
  if (docY < l.marginY) {
     _scrollY += docY - l.marginY;
  } else {
     int lines = std::max((size_t)1, _wrappedChoices[_selectedChoice].size());
     int textHeight = lines * l.choiceLineHeight;
     int currentChoiceHeight = std::max(l.minChoiceHeight, textHeight);
     if (docY + currentChoiceHeight > _display.getHeight() - l.marginY) {
        _scrollY += (docY + currentChoiceHeight) - (_display.getHeight() - l.marginY);
     }
  }
  
  if (_scrollY < 0) _scrollY = 0;
  if (_scrollY > _maxScrollY) _scrollY = _maxScrollY;
}

bool InkDisplayManager::isChoicesVisible() const {
  if (_numChoices == 0)
    return true;
  GfxRenderer *renderer = _display.getRenderer();
  if (!renderer)
    return true;

  // A choice is visible if the bottom of the scroll view reaches the choice
  // area. Actually, choices are at the bottom of the document.
  return (_scrollY >= _maxScrollY - 2);
}

bool InkDisplayManager::isRevealComplete() const {
  if (_numChoices == 0)
    return true;
  if (!_revealStarted)
    return false;
  return (_revealStep > _numChoices);
}

InkDisplayManager::ChoiceVisualState
InkDisplayManager::getChoiceVisualState(int index) const {
  if (index < 0 || index >= _numChoices)
    return ChoiceVisualState::FULL;
  if (!_revealStarted)
    return ChoiceVisualState::HIDDEN;
  if (index < _revealStep) {
    return ChoiceVisualState::FULL;
  } else {
    return ChoiceVisualState::HIDDEN;
  }
}

int InkDisplayManager::getChoiceIndexAtY(int touchY) const {
  if (_numChoices == 0 || !isRevealComplete())
    return -1;
  GfxRenderer *renderer = _display.getRenderer();
  if (!renderer)
    return -1;

  ChoiceLayout l = getChoiceLayout(renderer);

  // Start drawing Y from document start
  int docY = l.marginY - _scrollY;

  // Add narrative height
  for (const auto &w : _wrappedLines) {
    docY += w.getHeight(renderer);
  }

  // Choices start
  docY += (l.marginY / 2) + 2 + (l.marginY / 2); // separator

  for (int i = 0; i < _numChoices; ++i) {
    if (i > 0)
      docY += l.choicePadding;

    int lines = std::max((size_t)1, _wrappedChoices[i].size());
    int textHeight = lines * l.choiceLineHeight;
    int choiceBlockHeight = std::max(l.minChoiceHeight, textHeight);

    if (touchY >= docY && touchY < docY + choiceBlockHeight) {
      return i;
    }

    docY += choiceBlockHeight;
  }
  return -1;
}

void InkDisplayManager::redraw(InkStoryManager &storyManager,
                               AppSettings &settings, int &refreshCount) {
  _display.clear();

  GfxRenderer *renderer = _display.getRenderer();
  if (!renderer) {
    // Text-mode
    for (const auto &line : _wrappedLines) {
      std::string s;
      for (const auto &r : line.block.runs)
        s += r.text;
      _display.drawNarrativeLine(s.c_str());
    }
    if (_numChoices > 0) {
      _display.drawSeparator();
      for (int i = 0; i < _numChoices; ++i) {
        _display.drawChoiceLine(i, _choiceText[i], i == _selectedChoice);
      }
    }
    if (settings.refreshInterval > 0 &&
        refreshCount >= settings.refreshInterval) {
      refreshCount = 0;
      _display.fullRefresh();
    } else {
      _display.present();
    }
    return;
  }

  int width = _display.getWidth();
  int height = _display.getHeight();

  int marginX = settings.marginPx;
  int narrativeWidth = width - (2 * marginX);
  
  ChoiceLayout l = getChoiceLayout(renderer);

  int y = l.marginY - _scrollY;

  for (const auto &w : _wrappedLines) {
    int itemHeight = w.getHeight(renderer);
    if (!w.block.isEmpty() || w.isImage) {
      if ((y + itemHeight > 0) && (y < height)) {
        if (w.isImage) {
          InkStoryManager::ImageMeta meta;
          if (storyManager.getImageMeta(w.imagePath.c_str(), meta) &&
              storyManager.getMediaFile()) {
            ImageWidget::draw(*renderer, storyManager.getMediaFile(),
                              meta.offset, meta.size, meta.width, meta.height,
                              marginX, y, narrativeWidth, w.imageHeight);
          } else {
            renderer->setHalftone(true);
            renderer->fillRect(marginX, y, narrativeWidth, w.imageHeight, true);
            renderer->setHalftone(false);
          }
        } else {
          renderer->setHalftone(w.isOld);
          renderer->drawRichText(FONT_NARRATIVE, marginX, y, w.block);
          renderer->setHalftone(false);
        }
      }
    }
    y += itemHeight;
  }

  if (_numChoices > 0) {
    y += (l.marginY / 2);

    if ((y + 2 > 0) && (y < height)) {
      renderer->fillRect(marginX, y, narrativeWidth, 2, true);
    }
    y += (l.marginY / 2);

    int indicatorWidth = 24;

    for (int i = 0; i < _numChoices; ++i) {
      if (i > 0)
        y += l.choicePadding;

      bool selected = (i == _selectedChoice) && isRevealComplete();
      int choiceBlockLines = std::max((size_t)1, _wrappedChoices[i].size());
      int textHeight = choiceBlockLines * l.choiceLineHeight;
      int choiceBlockHeight = std::max(l.minChoiceHeight, textHeight);
      int verticalOffset = (choiceBlockHeight - textHeight) / 2;

      ChoiceVisualState state = getChoiceVisualState(i);
      if (state == ChoiceVisualState::HIDDEN) {
        y += choiceBlockHeight;
        continue;
      }

      if ((y + choiceBlockHeight > 0) && (y < height)) {
        if (selected) {
          renderer->fillRect(marginX - 4, y, narrativeWidth + 8,
                             choiceBlockHeight, true);
        }
      }

      if (_wrappedChoices[i].empty()) {
        if (selected) {
          renderer->drawText(FONT_CHOICE, marginX, y + verticalOffset, ">",
                             !selected);
        }
        y += choiceBlockHeight;
      } else {
        int textY = y + verticalOffset;
        for (size_t l_idx = 0; l_idx < _wrappedChoices[i].size(); ++l_idx) {
          if ((textY + l.choiceLineHeight > 0) && (textY < height)) {
            if (l_idx == 0 && selected) {
              renderer->drawText(FONT_CHOICE, marginX, textY, ">", !selected);
            }
            renderer->drawRichText(FONT_CHOICE, marginX + indicatorWidth, textY,
                                   _wrappedChoices[i][l_idx], !selected);
          }
          textY += l.choiceLineHeight;
        }
        y += choiceBlockHeight;
      }
    }
  }

  if (settings.refreshInterval > 0 &&
      refreshCount >= settings.refreshInterval) {
    refreshCount = 0;
    _display.fullRefresh();
  } else {
    _display.present();
  }
}
