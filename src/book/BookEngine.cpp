#include "book/BookEngine.h"
#include "render/PageRenderer.h"
#include "ui/SystemUI.h"
#include "ui/HeaderWidget.h"
#include "ui/ModalDialogWidget.h"
#include "ui/LoadingWidget.h"
#include "ui/QuickMenuWidget.h"
#include "ui/SettingsView.h"
#include "ui/MenuModalWidget.h"
#include "ui/NeuStyle.h"
#include "os/BootManager.h"
#include "GfxRenderer.h"
#include "hal/IDisplay.h"
#include "hal/IInput.h"

#ifdef PLATFORM_ESP32
#include <esp_heap_caps.h>
#include <FS.h>
#ifdef USE_SD_MMC
#include <SD_MMC.h>
#define SD_FS SD_MMC
#else
#include <SD.h>
#define SD_FS SD
#endif
#else
#include <cstdlib>
#include <cstdio>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#endif
#include <cstring>
#include <vector>

// Constants for arena size
#ifdef PLATFORM_ESP32
#ifdef BOARD_HAS_PSRAM
static constexpr size_t BOOK_ARENA_SIZE = 512 * 1024;
static constexpr size_t SCRATCH_ARENA_SIZE = 512 * 1024;
static constexpr size_t PARSE_ARENA_SIZE = 128 * 1024;
#else
static constexpr size_t BOOK_ARENA_SIZE = 12 * 1024;
static constexpr size_t SCRATCH_ARENA_SIZE = 32 * 1024;
static constexpr size_t PARSE_ARENA_SIZE = 48 * 1024;
#endif
#else
static constexpr size_t BOOK_ARENA_SIZE = 512 * 1024;
static constexpr size_t SCRATCH_ARENA_SIZE = 512 * 1024;
static constexpr size_t PARSE_ARENA_SIZE = 128 * 1024;
#endif

// A PageSink that captures the targeted page into temporary buffers
class CapturePageSink : public freeink::book::PageSink {
public:
    struct CapturedRun {
        freeink::book::PageTextRun run;
        std::string text;
    };
    struct CapturedImage {
        freeink::book::PageImage img;
        std::string href;
    };
    struct CapturedLink {
        freeink::book::PageLink link;
        std::string target;
        std::string fragment;
    };

    std::vector<CapturedRun> capturedRuns;
    std::vector<CapturedImage> capturedImages;
    std::vector<CapturedLink> capturedLinks;
    uint32_t pageIndex = 0;
    uint32_t charStart = 0;
    bool hasCaptured = false;
    uint32_t matchedIndex = 0;

    CapturePageSink(BookEngine::PageMatchMode mode, uint32_t targetVal) 
        : mode(mode), targetVal(targetVal) {}

    bool onPage(const freeink::book::Page& page) override {
        bool shouldCapture = false;
        if (mode == BookEngine::PageMatchMode::BY_INDEX) {
            if (page.pageIndex == targetVal) {
                shouldCapture = true;
                matchedIndex = page.pageIndex;
            }
        } else if (mode == BookEngine::PageMatchMode::LAST_PAGE) {
            shouldCapture = true;
            matchedIndex = page.pageIndex;
        } else if (mode == BookEngine::PageMatchMode::BY_CHAR_OFFSET) {
            if (page.charStart <= targetVal || !hasCaptured) {
                shouldCapture = true;
                matchedIndex = page.pageIndex;
            }
        }

        if (shouldCapture) {
            capturedRuns.clear();
            capturedImages.clear();
            capturedLinks.clear();
            pageIndex = page.pageIndex;
            charStart = page.charStart;

            for (uint16_t i = 0; i < page.runCount; i++) {
                CapturedRun cr;
                cr.run = page.runs[i];
                if (page.runs[i].text && page.runs[i].len > 0) {
                    cr.text.assign(page.runs[i].text, page.runs[i].len);
                }
                capturedRuns.push_back(cr);
            }
            for (uint16_t i = 0; i < page.imageCount; i++) {
                CapturedImage ci;
                ci.img = page.images[i];
                if (page.images[i].href) {
                    ci.href = page.images[i].href;
                }
                capturedImages.push_back(ci);
            }
            for (uint16_t i = 0; i < page.linkCount; i++) {
                CapturedLink cl;
                cl.link = page.links[i];
                if (page.links[i].target) cl.target = page.links[i].target;
                if (page.links[i].fragment) cl.fragment = page.links[i].fragment;
                capturedLinks.push_back(cl);
            }
            hasCaptured = true;
        }
        return true;
    }
    
    BookEngine::PageMatchMode mode = BookEngine::PageMatchMode::BY_INDEX;
    uint32_t targetVal = 0;
};

BookEngine::BookEngine(IDisplay& display, IInput& input)
    : _display(display), _input(input) {
    _bookmark.magic = Bookmark::MAGIC;
    _bookmark.spineIndex = 0;
    _bookmark.charOffset = 0;
}

void BookEngine::buildStem(char* out, size_t outLen) const {
    const char* slash = strrchr(_bookPath, '/');
    const char* base = slash ? slash + 1 : _bookPath;
    const char* dot = strrchr(base, '.');
    if (dot && (dot - base) < (int)outLen) {
        strncpy(out, base, dot - base);
        out[dot - base] = '\0';
    } else {
        strncpy(out, base, outLen - 1);
        out[outLen - 1] = '\0';
    }
}

BookEngine::~BookEngine() {
    for (int i = 0; i < RING_CACHE_SLOTS; i++) {
        if (_ringCache[i].framebuffer) {
#if defined(PLATFORM_ESP32) && defined(BOARD_HAS_PSRAM)
            heap_caps_free(_ringCache[i].framebuffer);
#else
            free(_ringCache[i].framebuffer);
#endif
            _ringCache[i].framebuffer = nullptr;
        }
    }
    if (_bookArenaBuf) {
#if defined(PLATFORM_ESP32) && defined(BOARD_HAS_PSRAM)
        heap_caps_free(_bookArenaBuf);
#else
        free(_bookArenaBuf);
#endif
        _bookArenaBuf = nullptr;
    }
    if (_scratchArenaBuf) {
#if defined(PLATFORM_ESP32) && defined(BOARD_HAS_PSRAM)
        heap_caps_free(_scratchArenaBuf);
#else
        free(_scratchArenaBuf);
#endif
        _scratchArenaBuf = nullptr;
    }
}

bool BookEngine::loadBook(const char* epubPath) {
    strncpy(_bookPath, epubPath, sizeof(_bookPath) - 1);

#if defined(PLATFORM_ESP32) && !defined(BOARD_HAS_PSRAM)
    _bookArenaBuf = (uint8_t*)malloc(BOOK_ARENA_SIZE);
    _scratchArenaBuf = nullptr;
    if (!_bookArenaBuf) {
        Serial.printf("[BookEngine] Arena allocation failed! bookArena: null (%u), free: %u, maxBlock: %u\n",
                      (unsigned)BOOK_ARENA_SIZE, (unsigned)ESP.getFreeHeap(), 
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return false;
    }
    _bookArena.init(_bookArenaBuf, BOOK_ARENA_SIZE);
#else
#if defined(PLATFORM_ESP32) && defined(BOARD_HAS_PSRAM)
    _bookArenaBuf = (uint8_t*)heap_caps_malloc(BOOK_ARENA_SIZE, MALLOC_CAP_SPIRAM);
    _scratchArenaBuf = (uint8_t*)heap_caps_malloc(SCRATCH_ARENA_SIZE, MALLOC_CAP_SPIRAM);
#else
    _bookArenaBuf = (uint8_t*)malloc(BOOK_ARENA_SIZE);
    _scratchArenaBuf = (uint8_t*)malloc(SCRATCH_ARENA_SIZE);
#endif

    if (!_bookArenaBuf || !_scratchArenaBuf) {
#ifdef PLATFORM_ESP32
        Serial.printf("[BookEngine] Arena allocation failed! bookArena: %p (%u), scratchArena: %p (%u), free: %u, maxBlock: %u\n",
                      _bookArenaBuf, (unsigned)BOOK_ARENA_SIZE, _scratchArenaBuf, (unsigned)SCRATCH_ARENA_SIZE, 
                      (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
        if (_bookArenaBuf) {
#if defined(PLATFORM_ESP32) && defined(BOARD_HAS_PSRAM)
            heap_caps_free(_bookArenaBuf);
#else
            free(_bookArenaBuf);
#endif
            _bookArenaBuf = nullptr;
        }
        if (_scratchArenaBuf) {
#if defined(PLATFORM_ESP32) && defined(BOARD_HAS_PSRAM)
            heap_caps_free(_scratchArenaBuf);
#else
            free(_scratchArenaBuf);
#endif
            _scratchArenaBuf = nullptr;
        }
        return false;
    }

    _bookArena.init(_bookArenaBuf, BOOK_ARENA_SIZE);
    _scratchArena.init(_scratchArenaBuf, SCRATCH_ARENA_SIZE);
#endif

    if (!_bookSource.open(epubPath)) {
        return false;
    }

    char stem[64] = {0};
    const char* slash = strrchr(epubPath, '/');
    const char* base = slash ? slash + 1 : epubPath;
    const char* dot = strrchr(base, '.');
    if (dot && (dot - base) < (int)sizeof(stem)) {
        strncpy(stem, base, dot - base);
    } else {
        strncpy(stem, base, sizeof(stem) - 1);
    }

    char cacheDir[128];
#ifdef PLATFORM_ESP32
    snprintf(cacheDir, sizeof(cacheDir), "/.eenk_cache/%s", stem);
#else
    snprintf(cacheDir, sizeof(cacheDir), ".eenk_cache/%s", stem);
#endif
    _cacheStorage.setBaseDir(cacheDir);

    uint8_t openScratchBuf[4096];
    freeink::book::Arena openScratch(openScratchBuf, sizeof(openScratchBuf));
    freeink::book::Arena& startupScratch = 
#if defined(PLATFORM_ESP32) && !defined(BOARD_HAS_PSRAM)
        openScratch;
#else
        _scratchArena;
#endif

    // Try opening existing catalog first
    freeink::book::BookStatus st = _catalog.open(_bookSource, _cacheStorage, _bookArena, startupScratch);
    if (st != freeink::book::BookStatus::Ok) {
#ifdef PLATFORM_ESP32
        Serial.printf("[BookEngine] Catalog not cached or stale (status: %d), building...\n", (int)st);
#endif
        _bookArena.reset();
#if !defined(PLATFORM_ESP32) || defined(BOARD_HAS_PSRAM)
        _scratchArena.reset();
#endif

#if defined(PLATFORM_ESP32) && !defined(BOARD_HAS_PSRAM)
        // On ESP32-C3 without PSRAM, internal DRAM is extremely tight.
        // Free reading buffer so Expat's XML_Parse has free heap for malloc.
        if (_bookArenaBuf) free(_bookArenaBuf);
        _bookArenaBuf = nullptr;

        // Allocate a compact 60 KB scratch buffer for the catalog build.
        // This leaves ~70 KB+ of contiguous heap free for Expat.
        constexpr size_t BUILD_SCRATCH_SIZE = 60 * 1024;
        uint8_t* buildBuf = (uint8_t*)malloc(BUILD_SCRATCH_SIZE);
        if (!buildBuf) {
            Serial.printf("[BookEngine] Failed to allocate build scratch (%u bytes)!\n", (unsigned)BUILD_SCRATCH_SIZE);
            return false;
        }
        freeink::book::Arena buildArena(buildBuf, BUILD_SCRATCH_SIZE);
        st = freeink::book::BookCatalog::build(_bookSource, _cacheStorage, buildArena, nullptr);
        free(buildBuf);

        if (st != freeink::book::BookStatus::Ok) {
            Serial.printf("[BookEngine] BookCatalog::build failed with status: %d\n", (int)st);
            return false;
        }

        // Re-allocate book arena now that Expat is done
        _bookArenaBuf = (uint8_t*)malloc(BOOK_ARENA_SIZE);
        if (!_bookArenaBuf) {
            Serial.printf("[BookEngine] Failed to reallocate book arena after build!\n");
            return false;
        }
        _bookArena.init(_bookArenaBuf, BOOK_ARENA_SIZE);
#else
        // On PSRAM / native targets, we have plenty of memory.
        // Pass _scratchArena as scratch, and _bookArena as parseArena.
        st = freeink::book::BookCatalog::build(_bookSource, _cacheStorage, _scratchArena, &_bookArena);
        if (st != freeink::book::BookStatus::Ok) {
#ifdef PLATFORM_ESP32
            Serial.printf("[BookEngine] BookCatalog::build failed with status: %d (failedAlloc: %u)\n", 
                          (int)st, (unsigned)_scratchArena.failedAllocSize());
#endif
            return false;
        }
        _bookArena.reset();
        _scratchArena.reset();
#endif

        st = _catalog.open(_bookSource, _cacheStorage, _bookArena, startupScratch);
        if (st != freeink::book::BookStatus::Ok) {
#ifdef PLATFORM_ESP32
            Serial.printf("[BookEngine] _catalog.open failed with status: %d (failedAlloc: %u)\n", 
                          (int)st, (unsigned)_bookArena.failedAllocSize());
#endif
            return false;
        }
    }
    _spineCount = _catalog.spineCount();

    if (!BookFontManager::setup(_fontSetup, startupScratch, _settings)) {
#ifdef PLATFORM_ESP32
        Serial.printf("[BookEngine] Failed to setup fonts!\n");
#endif
        return false;
    }
    _layoutParams.font = &_fontSetup.chain;

    applySettings(_settings);
    
    _generationHash = freeink::book::layoutGenerationHash(_layoutParams, 1);
    initRingCache();
    
    loadProgress();

    _state = State::PAGINATING;
    bool paginated = false;
    if (_bookmark.charOffset > 0) {
        paginated = paginateChapter(_currentSpine, PageMatchMode::BY_CHAR_OFFSET, _bookmark.charOffset);
    } else if (_bookmark.pageIndex > 0) {
        paginated = paginateChapter(_currentSpine, PageMatchMode::BY_INDEX, _bookmark.pageIndex);
    } else {
        paginated = paginateChapter(_currentSpine, PageMatchMode::BY_INDEX, 0);
    }
    if (!paginated) {
#ifdef PLATFORM_ESP32
        Serial.printf("[BookEngine] Failed to paginate initial chapter %u\n", _currentSpine);
#endif
        return false;
    }
    _state = State::READING;
    _needsRedraw = true;
    return true;
}

void BookEngine::applySettings(const AppSettings& settings) {
    _settings = settings;
    _layoutParams.pageWidth = _display.getWidth();
    _layoutParams.pageHeight = _display.getHeight();
    _layoutParams.marginLeft = settings.marginPx;
    _layoutParams.marginRight = settings.marginPx;
    _layoutParams.marginTop = settings.marginPx; 
    _layoutParams.marginBottom = settings.marginPx;
    _layoutParams.baseSizePx = 16;
    _layoutParams.defaultAlign = (settings.epubTextAlign == 0) ? freeink::book::TextAlign::Justify : freeink::book::TextAlign::Left;
    _layoutParams.hyphenator = settings.epubHyphenation ? &_fontSetup.hyphenator : nullptr;
}

bool BookEngine::paginateChapter(uint16_t spineIndex, PageMatchMode mode, uint32_t targetVal) {
    if (spineIndex >= _spineCount) return false;
    
    freeink::book::ZipEntry entry;
    _catalog.spineEntry(spineIndex, &entry);
    
    char href[128];
    _catalog.spineHref(spineIndex, href, sizeof(href));

    _totalPagesInChapter = 0;
    CapturePageSink sink(mode, targetVal);
    
#if defined(PLATFORM_ESP32) && !defined(BOARD_HAS_PSRAM)
    // Allocate the large parse buffer (48 KB) FIRST so it claims the largest free contiguous block.
    uint8_t* parseBuf = (uint8_t*)malloc(PARSE_ARENA_SIZE);
    // Allocate the layout scratch buffer (32 KB, with 30 KB fallback) SECOND from the remaining heap.
    size_t actualScratchSize = SCRATCH_ARENA_SIZE;
    uint8_t* scratchBuf = (uint8_t*)malloc(actualScratchSize);
    if (!scratchBuf && actualScratchSize > 30 * 1024) {
        actualScratchSize = 30 * 1024;
        scratchBuf = (uint8_t*)malloc(actualScratchSize);
    }

    if (!parseBuf || !scratchBuf) {
        Serial.printf("[BookEngine] paginateChapter alloc failed! parseBuf: %p (%u), scratchBuf: %p (%u), free: %u, maxBlock: %u\n",
                      parseBuf, (unsigned)PARSE_ARENA_SIZE, scratchBuf, (unsigned)actualScratchSize,
                      (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        if (parseBuf) free(parseBuf);
        if (scratchBuf) free(scratchBuf);
        return false;
    }

    freeink::book::Arena scratchArena(scratchBuf, actualScratchSize);
    freeink::book::Arena parseArena(parseBuf, PARSE_ARENA_SIZE);

    freeink::book::BookStatus status = freeink::book::ChapterLayout::layout(
        _bookSource, _catalog.zip(), entry, href, _layoutParams, 
        scratchArena, sink, &_totalPagesInChapter, nullptr, &parseArena
    );

    free(scratchBuf);
    free(parseBuf);
#else
    freeink::book::BookStatus status = freeink::book::ChapterLayout::layout(
        _bookSource, _catalog.zip(), entry, href, _layoutParams, 
        _scratchArena, sink, &_totalPagesInChapter, nullptr, nullptr
    );
#endif
    
    if (status == freeink::book::BookStatus::Ok && sink.hasCaptured) {
        _currentPage = sink.matchedIndex;
        if (_pageMark > 0) {
            _bookArena.release(_pageMark);
        }
        _pageMark = _bookArena.mark();
        
        _currentPageData.pageIndex = sink.pageIndex;
        _currentPageData.charStart = sink.charStart;
        _currentPageData.runCount = sink.capturedRuns.size();
        if (_currentPageData.runCount > 0) {
            auto* runs = _bookArena.allocArray<freeink::book::PageTextRun>(_currentPageData.runCount);
            if (runs) {
                for (size_t i = 0; i < sink.capturedRuns.size(); i++) {
                    runs[i] = sink.capturedRuns[i].run;
                    char* textCopy = (char*)_bookArena.alloc(sink.capturedRuns[i].text.length() + 1);
                    if (textCopy) {
                        memcpy(textCopy, sink.capturedRuns[i].text.data(), sink.capturedRuns[i].text.length());
                        textCopy[sink.capturedRuns[i].text.length()] = '\0';
                        runs[i].text = textCopy;
                    } else {
                        runs[i].text = nullptr;
                        runs[i].len = 0;
                    }
                }
                _currentPageData.runs = runs;
            } else {
                _currentPageData.runCount = 0;
                _currentPageData.runs = nullptr;
            }
        } else {
            _currentPageData.runs = nullptr;
        }

        _currentPageData.imageCount = sink.capturedImages.size();
        if (_currentPageData.imageCount > 0) {
            auto* imgs = _bookArena.allocArray<freeink::book::PageImage>(_currentPageData.imageCount);
            if (imgs) {
                for (size_t i = 0; i < sink.capturedImages.size(); i++) {
                    imgs[i] = sink.capturedImages[i].img;
                    if (!sink.capturedImages[i].href.empty()) {
                        imgs[i].href = _bookArena.strdup(sink.capturedImages[i].href.c_str());
                    } else {
                        imgs[i].href = nullptr;
                    }
                }
                _currentPageData.images = imgs;
            } else {
                _currentPageData.imageCount = 0;
                _currentPageData.images = nullptr;
            }
        } else {
            _currentPageData.images = nullptr;
        }

        _currentPageData.linkCount = sink.capturedLinks.size();
        if (_currentPageData.linkCount > 0) {
            auto* links = _bookArena.allocArray<freeink::book::PageLink>(_currentPageData.linkCount);
            if (links) {
                for (size_t i = 0; i < sink.capturedLinks.size(); i++) {
                    links[i] = sink.capturedLinks[i].link;
                    if (!sink.capturedLinks[i].target.empty()) links[i].target = _bookArena.strdup(sink.capturedLinks[i].target.c_str());
                    if (!sink.capturedLinks[i].fragment.empty()) links[i].fragment = _bookArena.strdup(sink.capturedLinks[i].fragment.c_str());
                }
                _currentPageData.links = links;
            } else {
                _currentPageData.linkCount = 0;
                _currentPageData.links = nullptr;
            }
        } else {
            _currentPageData.links = nullptr;
        }

        return true;
    }
#ifdef PLATFORM_ESP32
    Serial.printf("[BookEngine] paginateChapter failed with status: %d, hasCaptured: %d\n", 
                  (int)status, (int)sink.hasCaptured);
#endif
    return false;
}


void BookEngine::buildFbCachePath(char* out, size_t outLen) const {
    char stem[64] = {0};
    buildStem(stem, sizeof(stem));
#ifdef PLATFORM_ESP32
    snprintf(out, outLen, "/.eenk_cache/%s/fb_%u_%u_%u.bin", stem, (unsigned int)_currentSpine, (unsigned int)_currentPageData.charStart, (unsigned int)_generationHash);
#else
    snprintf(out, outLen, ".eenk_cache/%s/fb_%u_%u_%u.bin", stem, (unsigned int)_currentSpine, (unsigned int)_currentPageData.charStart, (unsigned int)_generationHash);
#endif
}

size_t BookEngine::getFramebufferSize() const {
    size_t panelWidth = _display.getHeight();
    size_t panelHeight = _display.getWidth();
    size_t panelWidthBytes = (panelWidth + 7) / 8;
    return panelWidthBytes * panelHeight;
}

void BookEngine::initRingCache() {
#if defined(PLATFORM_ESP32) && defined(BOARD_HAS_PSRAM)
    if (!_ringCacheEnabled) {
        for (int i = 0; i < RING_CACHE_SLOTS; i++) {
            _ringCache[i].framebuffer = (uint8_t*)heap_caps_malloc(MAX_FB_SIZE, MALLOC_CAP_SPIRAM);
            _ringCache[i].valid = false;
            if (!_ringCache[i].framebuffer) {
                // Failed to allocate - disable ring cache
                for (int j = 0; j < i; j++) {
                    heap_caps_free(_ringCache[j].framebuffer);
                    _ringCache[j].framebuffer = nullptr;
                }
                return;
            }
        }
        _ringCacheEnabled = true;
    }
#endif
}

bool BookEngine::loadFromRingCache() {
    if (!_ringCacheEnabled) return false;
    for (int i = 0; i < RING_CACHE_SLOTS; i++) {
        if (_ringCache[i].valid && 
            _ringCache[i].spineIndex == _currentSpine && 
            _ringCache[i].charStart == _currentPageData.charStart) {
            uint8_t* fb = _display.getRenderer()->getFrameBuffer();
            if (fb) {
                memcpy(fb, _ringCache[i].framebuffer, getFramebufferSize());
                return true;
            }
        }
    }
    return false;
}

void BookEngine::saveToRingCache() {
    if (!_ringCacheEnabled) return;
    uint8_t* fb = _display.getRenderer()->getFrameBuffer();
    if (fb) {
        memcpy(_ringCache[_ringCacheNext].framebuffer, fb, getFramebufferSize());
        _ringCache[_ringCacheNext].spineIndex = _currentSpine;
        _ringCache[_ringCacheNext].charStart = _currentPageData.charStart;
        _ringCache[_ringCacheNext].valid = true;
        _ringCacheNext = (_ringCacheNext + 1) % RING_CACHE_SLOTS;
    }
}

bool BookEngine::loadCachedFramebuffer() {
    char path[256];
    buildFbCachePath(path, sizeof(path));
    uint8_t* fb = _display.getRenderer()->getFrameBuffer();
    if (!fb) return false;
    size_t fbSize = getFramebufferSize();

#ifdef PLATFORM_ESP32
    File f = SD_FS.open(path, FILE_READ);
    if (!f) return false;
    size_t readLen = f.read(fb, fbSize);
    f.close();
    return readLen == fbSize;
#else
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t readLen = fread(fb, 1, fbSize, f);
    fclose(f);
    return readLen == fbSize;
#endif
}

void BookEngine::saveCachedFramebuffer() {
    char path[256];
    buildFbCachePath(path, sizeof(path));
    uint8_t* fb = _display.getRenderer()->getFrameBuffer();
    if (!fb) return;
    size_t fbSize = getFramebufferSize();

    char stem[64] = {0};
    buildStem(stem, sizeof(stem));

#ifdef PLATFORM_ESP32
    char dir[128];
    snprintf(dir, sizeof(dir), "/.eenk_cache/%s", stem);
    if (!SD_FS.exists(dir)) {
        SD_FS.mkdir(dir);
    }
    File f = SD_FS.open(path, FILE_WRITE);
    if (f) {
        f.write(fb, fbSize);
        f.close();
    }
#else
    char dir[128];
    snprintf(dir, sizeof(dir), ".eenk_cache/%s", stem);
#ifdef _WIN32
    mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(fb, 1, fbSize, f);
        fclose(f);
    }
#endif
}

void BookEngine::showLoadingIndicator() {
    _display.clear();
    GfxRenderer* renderer = _display.getRenderer();
    if (renderer) {
        int fontId = NeuStyle::FONT_BODY;
        int lineH = renderer->getLineHeight(fontId);
        int y = (_display.getHeight() - lineH) / 2;
        renderer->drawCenteredText(fontId, y, "Loading...");
    }
    _display.present();
}

void BookEngine::generateCoverThumbnail() {
    uint8_t* fb = _display.getRenderer()->getFrameBuffer();
    if (!fb) return;
    
    char path[256];
    char stem[64] = {0};
    buildStem(stem, sizeof(stem));
#ifdef PLATFORM_ESP32
    snprintf(path, sizeof(path), "/.eenk_cache/%s/thumb.bin", stem);
#else
    snprintf(path, sizeof(path), ".eenk_cache/%s/thumb.bin", stem);
#endif

    const int thumbBox = 152;
    int srcW = _display.getWidth();
    int srcH = _display.getHeight();
    
    // Calculate proportional scaling
    float scale = (float)thumbBox / srcH; 
    int drawW = (int)(srcW * scale);
    int drawH = thumbBox;

    int panelWidth = srcH;
    int panelHeight = srcW;
    GfxRenderer::Orientation o = _display.getRenderer() ? _display.getRenderer()->getOrientation() : GfxRenderer::PortraitInverted;
    if (o == GfxRenderer::LandscapeClockwise || o == GfxRenderer::LandscapeCounterClockwise) {
        panelWidth = srcW;
        panelHeight = srcH;
    }
    int panelWidthBytes = (panelWidth + 7) / 8;
    
    size_t outRowBytes = (drawW + 7) / 8;
    size_t outSize = outRowBytes * drawH;
    uint8_t* outBuf = (uint8_t*)malloc(outSize);
    if (!outBuf) return;
    memset(outBuf, 0xFF, outSize); // initialize to white

    // Floyd-Steinberg dithering with 2 error rows (minimal RAM usage)
    float currErrors[256] = {0};
    float nextErrors[256] = {0};

    // Box filter + Floyd-Steinberg error diffusion: compute average grayscale for each dest pixel
    for (int ty = 0; ty < drawH; ty++) {
        for (int tx = 0; tx < drawW; tx++) {
            int srcXStart = (tx * srcW) / drawW;
            int srcXEnd = ((tx + 1) * srcW) / drawW;
            int srcYStart = (ty * srcH) / drawH;
            int srcYEnd = ((ty + 1) * srcH) / drawH;
            
            if (srcXEnd == srcXStart) srcXEnd++;
            if (srcYEnd == srcYStart) srcYEnd++;
            
            int total = 0;
            int black = 0;
            
            for (int sy = srcYStart; sy < srcYEnd; sy++) {
                for (int sx = srcXStart; sx < srcXEnd; sx++) {
                    int panelX, panelY;
                    switch(o) {
                        case GfxRenderer::Portrait: panelX = sy; panelY = panelHeight - 1 - sx; break;
                        case GfxRenderer::PortraitInverted: panelX = (panelWidth - 1) - sy; panelY = sx; break;
                        case GfxRenderer::LandscapeClockwise: panelX = (panelWidth - 1) - sx; panelY = (panelHeight - 1) - sy; break;
                        case GfxRenderer::LandscapeCounterClockwise: panelX = sx; panelY = sy; break;
                        default: panelX = (panelWidth - 1) - sy; panelY = sx; break;
                    }
                    if (panelX < 0) panelX = 0;
                    if (panelX > panelWidth - 1) panelX = panelWidth - 1;
                    if (panelY < 0) panelY = 0;
                    if (panelY > panelHeight - 1) panelY = panelHeight - 1;
                    
                    int bitIdx = 7 - (panelX & 7);
                    int byteIdx = panelY * panelWidthBytes + (panelX / 8);
                    total++;
                    if (!(fb[byteIdx] & (1 << bitIdx))) {
                        black++;
                    }
                }
            }
            
            float rawGray = (total > 0) ? (255.0f * (1.0f - (float)black / (float)total)) : 255.0f;
            float oldPixel = rawGray + currErrors[tx];
            float newPixel = (oldPixel < 128.0f) ? 0.0f : 255.0f;
            
            if (newPixel == 0.0f) {
                outBuf[ty * outRowBytes + (tx / 8)] &= ~(1 << (7 - (tx & 7)));
            }

            float err = oldPixel - newPixel;
            if (tx + 1 < drawW && tx + 1 < 256) {
                currErrors[tx + 1] += err * (7.0f / 16.0f);
            }
            if (ty + 1 < drawH) {
                if (tx > 0) nextErrors[tx - 1] += err * (3.0f / 16.0f);
                if (tx < 256) nextErrors[tx] += err * (5.0f / 16.0f);
                if (tx + 1 < drawW && tx + 1 < 256) nextErrors[tx + 1] += err * (1.0f / 16.0f);
            }
        }
        memcpy(currErrors, nextErrors, sizeof(currErrors));
        memset(nextErrors, 0, sizeof(nextErrors));
    }
    
    // 8-byte header: uint16_t width, uint16_t height, uint32_t reserved
    uint16_t hdr[4] = { (uint16_t)drawW, (uint16_t)drawH, 0, 0 };
    
#ifdef PLATFORM_ESP32
    File f = SD_FS.open(path, FILE_WRITE);
    if (f) {
        f.write((const uint8_t*)hdr, 8);
        f.write(outBuf, outSize);
        f.close();
    }
#else
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(hdr, 1, 8, f);
        fwrite(outBuf, 1, outSize, f);
        fclose(f);
    }
#endif
    free(outBuf);
}

bool BookEngine::getCoverThumbPath(const char* epubPath, char* out, size_t outLen) {
    char stem[64] = {0};
    const char* slash = strrchr(epubPath, '/');
    const char* base = slash ? slash + 1 : epubPath;
    const char* dot = strrchr(base, '.');
    if (dot && (dot - base) < (int)sizeof(stem)) {
        strncpy(stem, base, dot - base);
    } else {
        strncpy(stem, base, sizeof(stem) - 1);
    }
#ifdef PLATFORM_ESP32
    snprintf(out, outLen, "/.eenk_cache/%s/thumb.bin", stem);
#else
    snprintf(out, outLen, ".eenk_cache/%s/thumb.bin", stem);
#endif
    return true;
}


void BookEngine::renderCurrentPage() {
    // 1. Check PSRAM ring cache
    if (loadFromRingCache()) return;
    
    // 2. Check SD framebuffer cache
    if (_currentPageData.imageCount > 0 && loadCachedFramebuffer()) {
        saveToRingCache();
        if (_currentSpine == 0 && _currentPage == 0) {
            generateCoverThumbnail();
        }
        return;
    }
    
    // 3. Show loading indicator for image-heavy pages
    if (_currentPageData.imageCount > 0) {
        showLoadingIndicator();
    }
    
    // 4. Full decode (existing code)
    _display.clear();
    
    freeink::book::FrameTarget target;
    target.framebuffer = _display.getRenderer()->getFrameBuffer();
    target.width = _display.getHeight(); // physical panel-native (e.g. 800 or 792)
    target.height = _display.getWidth(); // physical panel-native (e.g. 480 or 528)
    target.widthBytes = (target.width + 7) / 8;
    target.format = freeink::book::FrameFormat::Mono1Dithered;
    
    GfxRenderer::Orientation o = _display.getRenderer() ? _display.getRenderer()->getOrientation() : GfxRenderer::Portrait;
    switch (o) {
        case GfxRenderer::Portrait:
            target.rotation = freeink::book::FrameRotation::Portrait;
            break;
        case GfxRenderer::PortraitInverted:
            target.rotation = freeink::book::FrameRotation::PortraitInverted;
            break;
        case GfxRenderer::LandscapeClockwise:
            target.rotation = freeink::book::FrameRotation::UpsideDown;
            break;
        case GfxRenderer::LandscapeCounterClockwise:
            target.rotation = freeink::book::FrameRotation::None;
            break;
    }
    
    freeink::book::PageRenderer::renderText(_currentPageData, _fontSetup.chain, target, nullptr);

    freeink::book::BookStatus imgStatus = freeink::book::BookStatus::Ok;
#if defined(PLATFORM_ESP32) && !defined(BOARD_HAS_PSRAM)
    if (_currentPageData.imageCount > 0) {
        uint8_t* renderScratchBuf = (uint8_t*)malloc(SCRATCH_ARENA_SIZE);
        if (renderScratchBuf) {
            freeink::book::Arena renderScratch(renderScratchBuf, SCRATCH_ARENA_SIZE);
            imgStatus = freeink::book::PageRenderer::renderImages(_currentPageData, _bookSource, _catalog.zip(), renderScratch, target);
            if (_currentPageData.imageCount > 0) {
                Serial.printf("[BookEngine] renderImages status: %d (scratch failedAlloc: %u)\n", 
                              (int)imgStatus, (unsigned)renderScratch.failedAllocSize());
            }
            free(renderScratchBuf);
        } else {
            Serial.printf("[BookEngine] Failed to allocate scratch for renderImages (%u bytes)\n", (unsigned)SCRATCH_ARENA_SIZE);
            imgStatus = freeink::book::BookStatus::OutOfMemory;
        }
    }
#else
    _scratchArena.reset();
    imgStatus = freeink::book::PageRenderer::renderImages(_currentPageData, _bookSource, _catalog.zip(), _scratchArena, target);
#ifdef PLATFORM_ESP32
    if (_currentPageData.imageCount > 0) {
        Serial.printf("[BookEngine] renderImages status: %d (scratch failedAlloc: %u)\n", 
                      (int)imgStatus, (unsigned)_scratchArena.failedAllocSize());
    }
#endif
#endif

    // 5. Cache the result
    if (_currentPageData.imageCount > 0 && imgStatus == freeink::book::BookStatus::Ok) {
        saveCachedFramebuffer();
    }
    saveToRingCache();
    
    // 6. Generate cover thumbnail on first render of cover page
    if (_currentSpine == 0 && _currentPage == 0 && (_currentPageData.imageCount == 0 || imgStatus == freeink::book::BookStatus::Ok)) {
        generateCoverThumbnail();
    }
}

void BookEngine::drawChrome() {
    // Header bar is only visible in exit modal and quick settings menu, matching ink story player
}

void BookEngine::handleInput() {
    uint32_t now = millis();
    if (_lastActionTime > 0 && (now - _lastActionTime < 300)) {
        return;
    }

    ButtonEvent ev = _input.pollInput();
    int touchX = -1, touchY = -1;
    bool hasTouchTap = _input.getTouchPosition(touchX, touchY) && touchX >= 0 && touchY >= 0;

    if (ev == ButtonEvent::NONE && !hasTouchTap) return;

    _input.resetActivityTimer();

    if (ev == ButtonEvent::TOP_EDGE_SWIPE) {
        BatteryMonitor *dummyBm = nullptr;
        BatteryWidget stackBw(*_display.getRenderer(), *dummyBm);
        BatteryWidget &bw = _batteryWidget ? *_batteryWidget : stackBw;
        QuickMenuWidget qm(_display, _input, bw, _frontlight, _settings, false);
        QuickMenuAction act = qm.show();
        if (act == QuickMenuAction::SLEEP_DEVICE) {
            saveProgress();
            setShouldSleep(true);
            return;
        }
        _needsRedraw = true;
        _lastActionTime = millis();
        return;
    }

    if (hasTouchTap) {
        if (_settings.touchScrollEnabled) {
            _lastActionTime = millis();
            if (touchX < _display.getWidth() / 3) {
                // Tap left 1/3 -> previous page
                prevPage();
            } else {
                // Tap right 2/3 -> next page
                nextPage();
            }
        }
        return;
    }

    if (ev == ButtonEvent::SWIPE_RIGHT) {
        if (_settings.touchScrollEnabled) {
            _lastActionTime = millis();
            prevPage();
        }
    } else if (ev == ButtonEvent::SWIPE_LEFT) {
        if (_settings.touchScrollEnabled) {
            _lastActionTime = millis();
            nextPage();
        }
    } else if (ev == ButtonEvent::UP || ev == ButtonEvent::LEFT) {
        _lastActionTime = millis();
        prevPage();
    } else if (ev == ButtonEvent::DOWN || ev == ButtonEvent::RIGHT || ev == ButtonEvent::CONFIRM) {
        _lastActionTime = millis();
        nextPage();
    } else if (ev == ButtonEvent::BACK || ev == ButtonEvent::QUIT) {
        _lastActionTime = millis();
        showReaderMenu();
    } else if (ev == ButtonEvent::SLEEP) {
        saveProgress();
        setShouldSleep(true);
    }
}

void BookEngine::nextPage() {
    if (_currentPage + 1 < _totalPagesInChapter) {
        _currentPage++;
        paginateChapter(_currentSpine, PageMatchMode::BY_INDEX, _currentPage);
        _needsRedraw = true;
    } else {
        nextChapter();
    }
}

void BookEngine::prevPage() {
    if (_currentPage > 0) {
        _currentPage--;
        paginateChapter(_currentSpine, PageMatchMode::BY_INDEX, _currentPage);
        _needsRedraw = true;
    } else {
        prevChapter();
    }
}

void BookEngine::nextChapter() {
    if (_currentSpine + 1 < _spineCount) {
        _currentSpine++;
        _currentPage = 0;
        paginateChapter(_currentSpine, PageMatchMode::BY_INDEX, 0);
        _needsRedraw = true;
    }
}

void BookEngine::prevChapter() {
    if (_currentSpine > 0) {
        _currentSpine--;
        paginateChapter(_currentSpine, PageMatchMode::LAST_PAGE, 0);
        _needsRedraw = true;
    }
}

int BookEngine::getProgressPercentage() const {
    if (!_catalog.isOpen() || _spineCount == 0) return 0;
    
    uint64_t totalBytes = 0;
    for (size_t i = 0; i < _spineCount; ++i) {
        totalBytes += _catalog.spineSize(i);
    }
    if (totalBytes == 0) return 0;
    
    uint64_t readBytes = 0;
    for (size_t i = 0; i < _currentSpine; ++i) {
        readBytes += _catalog.spineSize(i);
    }
    
    if (_totalPagesInChapter > 0 && _currentSpine < _spineCount) {
        uint64_t currentSpineBytes = _catalog.spineSize(_currentSpine);
        readBytes += (currentSpineBytes * (_currentPage + 1)) / _totalPagesInChapter;
    }
    
    int pct = static_cast<int>((readBytes * 100) / totalBytes);
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    return pct;
}

void BookEngine::getChapterHeaderString(char* out, size_t outLen) const {
    if (!out || outLen == 0) return;
    out[0] = '\0';

    char chapterTitle[96] = {0};
    char fragBuf[32] = {0};

    // 1. Try to find chapter title from TOC for current spine
    int tocIdx = _catalog.isOpen() ? _catalog.tocIndexForSpine(_currentSpine) : -1;
    if (tocIdx >= 0) {
        freeink::book::BookCatalog::TocItem item;
        if (_catalog.tocItem(tocIdx, &item, chapterTitle, sizeof(chapterTitle), fragBuf, sizeof(fragBuf)) == freeink::book::BookStatus::Ok) {
            // Trim leading/trailing whitespace
            char* p = chapterTitle;
            while (*p == ' ' || *p == '\t') p++;
            if (p != chapterTitle) {
                memmove(chapterTitle, p, strlen(p) + 1);
            }
            size_t l = strlen(chapterTitle);
            while (l > 0 && (chapterTitle[l - 1] == ' ' || chapterTitle[l - 1] == '\t' || chapterTitle[l - 1] == '\r' || chapterTitle[l - 1] == '\n')) {
                chapterTitle[l - 1] = '\0';
                l--;
            }
        }
    }

    // Fallback if no chapter title in TOC
    if (chapterTitle[0] == '\0') {
        if (_spineCount > 0) {
            snprintf(chapterTitle, sizeof(chapterTitle), "Chapter %u", _currentSpine + 1);
        } else {
            snprintf(chapterTitle, sizeof(chapterTitle), "Book");
        }
    }

    char suffix[48] = {0};
    int pct = getProgressPercentage();
    if (_totalPagesInChapter > 0) {
        snprintf(suffix, sizeof(suffix), " - p. %u/%u (%d%%)", 
                 (unsigned int)(_currentPage + 1), (unsigned int)_totalPagesInChapter, pct);
    } else {
        snprintf(suffix, sizeof(suffix), " - %d%%", pct);
    }

    // Check if combined string fits display header, trimming chapterTitle if needed
    auto* r = _display.getRenderer();
    int maxW = _display.getWidth() - 180;
    if (r && maxW > 100) {
        int fontHeading = NeuStyle::FONT_HEADING;
        std::string upperSuffix(suffix);
        for (auto &c : upperSuffix) c = toupper(c);
        int suffixW = r->getTextWidth(fontHeading, upperSuffix.c_str());
        int dotsW = r->getTextWidth(fontHeading, "...");
        int targetW = maxW - suffixW - dotsW;
        if (targetW < 0) targetW = 0;

        std::string upperTitle(chapterTitle);
        for (auto &c : upperTitle) c = toupper(c);

        if (r->getTextWidth(fontHeading, upperTitle.c_str()) > (maxW - suffixW)) {
            size_t len = strlen(chapterTitle);
            while (len > 0 && r->getTextWidth(fontHeading, upperTitle.c_str()) > targetW) {
                len--;
                chapterTitle[len] = '\0';
                upperTitle.resize(len);
            }
            while (len > 0 && chapterTitle[len - 1] == ' ') {
                len--;
                chapterTitle[len] = '\0';
            }
            if (len > 0) {
                strncat(chapterTitle, "...", sizeof(chapterTitle) - strlen(chapterTitle) - 1);
            }
        }
    }

    snprintf(out, outLen, "%s%s", chapterTitle, suffix);
}

void BookEngine::showReaderMenu() {
    std::vector<std::string> options;
    options.push_back("Table of Contents");
    options.push_back("Exit to Library");
    options.push_back("Resume");

    char headerBuf[128] = {0};
    getChapterHeaderString(headerBuf, sizeof(headerBuf));

    int choice = MenuModalWidget::show(_display, _input, _batteryWidget, "Book Menu",
                                       options, 0, headerBuf, 0, 0, true, nullptr, nullptr, 3);
    if (choice == 0) {
        showTableOfContents();
    } else if (choice == 1) {
        if (showConfirmExit()) {
            exitToMenu();
        } else {
            _needsRedraw = true;
        }
    } else {
        _needsRedraw = true;
    }
}

void BookEngine::showTableOfContents() {
    size_t count = _catalog.tocCount();
    std::vector<std::string> tocItems;
    std::vector<int> spineIndices;

    char titleBuf[128];
    char fragBuf[64];

    if (count > 0) {
        bool loadedFromCache = false;
        if (_cacheStorage.exists("toc.fibc")) {
            int64_t size = _cacheStorage.fileSize("toc.fibc");
            if (size > 0 && size < 65536) { // Max 64KB
                uint8_t* buf = (uint8_t*)malloc(size);
                if (buf) {
                    if (_cacheStorage.readAt("toc.fibc", 0, buf, size) == size) {
                        uint32_t offset = 0;
                        while (offset < size) {
                            int spineIdx;
                            uint16_t len;
                            if (offset + sizeof(int) + sizeof(uint16_t) > size) break;
                            memcpy(&spineIdx, buf + offset, sizeof(int));
                            offset += sizeof(int);
                            memcpy(&len, buf + offset, sizeof(uint16_t));
                            offset += sizeof(uint16_t);
                            if (offset + len > size) break;
                            std::string label((char*)(buf + offset), len);
                            offset += len;
                            
                            tocItems.push_back(label);
                            spineIndices.push_back(spineIdx);
                        }
                        loadedFromCache = true;
                    }
                    free(buf);
                }
            }
        }

        if (!loadedFromCache) {
            for (size_t i = 0; i < count; i++) {
                freeink::book::BookCatalog::TocItem item;
                if (_catalog.tocItem(i, &item, titleBuf, sizeof(titleBuf), fragBuf, sizeof(fragBuf)) == freeink::book::BookStatus::Ok) {
                    std::string label;
                    for (uint8_t d = 0; d < item.depth && d < 6; d++) {
                        label += "  ";
                    }
                    label += (titleBuf[0] != '\0') ? titleBuf : "Untitled";
                    tocItems.push_back(label);
                    spineIndices.push_back(item.spineIndex);
                }
            }
            
            if (_cacheStorage.beginWrite("toc.fibc")) {
                for (size_t i = 0; i < tocItems.size(); i++) {
                    int spineIdx = spineIndices[i];
                    uint16_t len = tocItems[i].length();
                    _cacheStorage.write(&spineIdx, sizeof(int));
                    _cacheStorage.write(&len, sizeof(uint16_t));
                    _cacheStorage.write(tocItems[i].data(), len);
                }
                _cacheStorage.endWrite();
            }
        }
    }

    // Fallback if no TOC entries found
    if (tocItems.empty()) {
        for (uint16_t s = 0; s < _spineCount; s++) {
            char label[64];
            snprintf(label, sizeof(label), "Chapter %u", s + 1);
            tocItems.push_back(label);
            spineIndices.push_back(s);
        }
    }

    if (tocItems.empty()) return;

    // Determine initial selection
    int initialSelection = 0;
    if (count > 0) {
        int tocIdx = _catalog.tocIndexForSpine(_currentSpine);
        if (tocIdx >= 0 && tocIdx < static_cast<int>(tocItems.size())) {
            initialSelection = tocIdx;
        }
    } else {
        if (_currentSpine < static_cast<uint16_t>(spineIndices.size())) {
            initialSelection = _currentSpine;
        }
    }

    char headerBuf[128] = {0};
    getChapterHeaderString(headerBuf, sizeof(headerBuf));

    bool isTouch = _settings.touchChoicesEnabled;
    int maxItems = 6;
    int itemMinH = 48;

    if (_display.getHeight() >= 700) {
        if (isTouch) {
            maxItems = 5;
            itemMinH = 64; // Spacious touch targets for X4 Pro
        } else {
            maxItems = 7;
            itemMinH = 48; // Compact items for X4 (physical buttons)
        }
    } else {
        if (isTouch) {
            maxItems = 3;
            itemMinH = 56;
        } else {
            maxItems = 4;
            itemMinH = 40;
        }
    }

    int chosen = MenuModalWidget::show(
        _display, _input, _batteryWidget, "Table of Contents",
        tocItems, initialSelection, headerBuf, 0, 0, true, nullptr, nullptr,
        maxItems,
        itemMinH
    );

    if (chosen >= 0 && chosen < static_cast<int>(spineIndices.size())) {
        int targetSpine = spineIndices[chosen];
        if (targetSpine >= 0 && targetSpine < static_cast<int>(_spineCount)) {
            _pendingSpineJump = targetSpine;
        }
    }
    _needsRedraw = true;
}

bool BookEngine::showConfirmExit() {
    char progressStr[32];
    snprintf(progressStr, sizeof(progressStr), "%d%%", getProgressPercentage());
    return ModalDialogWidget::show(_display, _input, _batteryWidget, "Exit Reader",
                                   "Do you want to exit to the menu?", progressStr);
}

void BookEngine::saveProgress() {
    _bookmark.magic = Bookmark::MAGIC;
    _bookmark.spineIndex = _currentSpine;
    _bookmark.padding = 0;
    _bookmark.pageIndex = _currentPage;
    _bookmark.charOffset = _currentPageData.charStart;
    
    char stem[64] = {0};
    buildStem(stem, sizeof(stem));
    
    char savePath[128];
#ifdef PLATFORM_ESP32
    if (!SD_FS.exists("/.eenk_saves")) {
        SD_FS.mkdir("/.eenk_saves");
    }
    snprintf(savePath, sizeof(savePath), "/.eenk_saves/%s.sav", stem);
    File f = SD_FS.open(savePath, FILE_WRITE);
    if (f) {
        f.write(reinterpret_cast<const uint8_t*>(&_bookmark), sizeof(_bookmark));
        f.close();
    }
#else
#ifdef _WIN32
    mkdir(".eenk_saves");
#else
    mkdir(".eenk_saves", 0755);
#endif
    snprintf(savePath, sizeof(savePath), ".eenk_saves/%s.sav", stem);
    FILE* f = fopen(savePath, "wb");
    if (f) {
        fwrite(&_bookmark, 1, sizeof(_bookmark), f);
        fclose(f);
    }
#endif
}

void BookEngine::loadProgress() {
    char stem[64] = {0};
    buildStem(stem, sizeof(stem));

    uint8_t buf[sizeof(Bookmark)] = {0};
    char savePath[128];
#ifdef PLATFORM_ESP32
    snprintf(savePath, sizeof(savePath), "/.eenk_saves/%s.sav", stem);
    File f = SD_FS.open(savePath, FILE_READ);
    if (f) {
        size_t n = f.read(buf, sizeof(buf));
        if (n >= 12) {
            uint32_t magic = *reinterpret_cast<uint32_t*>(buf);
            if (magic == Bookmark::MAGIC) {
                _bookmark.spineIndex = *reinterpret_cast<uint16_t*>(buf + 4);
                if (n >= 16) {
                    _bookmark.pageIndex = *reinterpret_cast<uint32_t*>(buf + 8);
                    _bookmark.charOffset = *reinterpret_cast<uint32_t*>(buf + 12);
                } else {
                    _bookmark.pageIndex = 0;
                    _bookmark.charOffset = *reinterpret_cast<uint32_t*>(buf + 8);
                }
                _currentSpine = _bookmark.spineIndex;
                _currentPage = _bookmark.pageIndex;
            }
        }
        f.close();
    }
#else
    snprintf(savePath, sizeof(savePath), ".eenk_saves/%s.sav", stem);
    FILE* f = fopen(savePath, "rb");
    if (f) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n >= 12) {
            uint32_t magic = *reinterpret_cast<uint32_t*>(buf);
            if (magic == Bookmark::MAGIC) {
                _bookmark.spineIndex = *reinterpret_cast<uint16_t*>(buf + 4);
                if (n >= 16) {
                    _bookmark.pageIndex = *reinterpret_cast<uint32_t*>(buf + 8);
                    _bookmark.charOffset = *reinterpret_cast<uint32_t*>(buf + 12);
                } else {
                    _bookmark.pageIndex = 0;
                    _bookmark.charOffset = *reinterpret_cast<uint32_t*>(buf + 8);
                }
                _currentSpine = _bookmark.spineIndex;
                _currentPage = _bookmark.pageIndex;
            }
        }
        fclose(f);
    }
#endif
}

void BookEngine::exitToMenu() {
    LoadingWidget::show(_display, "Saving Progress...", 1.0f);
    delay(100);
    saveProgress();
    LoadingWidget::show(_display, "Loading...", 1.0f);
    BootManager::setBootMode(BootMode::MENU);
    delay(500);
    BootManager::reboot();
}

void BookEngine::update() {
    if (_state == State::READING) {
        handleInput();
        
        if (_pendingSpineJump != -1) {
            uint16_t target = _pendingSpineJump;
            _pendingSpineJump = -1;
            
            _currentSpine = target;
            _currentPage = 0;
            paginateChapter(_currentSpine, PageMatchMode::BY_INDEX, 0);
            _needsRedraw = true;
        }

        if (_needsRedraw) {
            renderCurrentPage();
            drawChrome();
            _display.present();
            _needsRedraw = false;
        }
    }
    delay(16);
}

bool BookEngine::isDone() const {
    return _state == State::DONE;
}

bool BookEngine::shouldSleep() const {
    return _shouldSleep;
}

void BookEngine::setShouldSleep(bool sleep) {
    _shouldSleep = sleep;
}

void BookEngine::setFrontlight(IFrontlight* fl) {
    _frontlight = fl;
}

void BookEngine::setBatteryWidget(BatteryWidget* bw) {
    _batteryWidget = bw;
}
