#pragma once

#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "os/AppSettings.h"
#include "book/SdBookSource.h"
#include "book/SdCacheStorage.h"
#include "BookCatalog.h"
#include "layout/ChapterLayout.h"
#include "BookFontManager.h"
#include "ui/BatteryWidget.h"
#include "cache/PageCache.h"
#include <cstdint>

class IFrontlight;
class BatteryWidget;

class BookEngine {
public:
    BookEngine(IDisplay& display, IInput& input);
    ~BookEngine();

    bool loadBook(const char* epubPath);
    void applySettings(const AppSettings& settings);
    void update();  // called each frame
    bool isDone() const;
    bool shouldSleep() const;
    void setShouldSleep(bool sleep);
    void setFrontlight(IFrontlight* fl);
    void setBatteryWidget(BatteryWidget* bw);
    int getProgressPercentage() const;

    enum class PageMatchMode { BY_INDEX, BY_CHAR_OFFSET, LAST_PAGE };

    // Make capturePage public so the PageSink can access it
    void capturePage(const freeink::book::Page& page);

    // Get the cover thumbnail path for library use
    static bool getCoverThumbPath(const char* epubPath, char* out, size_t outLen);

private:
    enum class State { LOADING, PAGINATING, READING, DONE };
    
    IDisplay& _display;
    IInput& _input;
    IFrontlight* _frontlight = nullptr;
    BatteryWidget* _batteryWidget = nullptr;
    AppSettings _settings;
    
    // Storage adapters
    eenk::book::SdBookSource _bookSource;
    eenk::book::SdCacheStorage _cacheStorage;
    
    // Arena memory
    uint8_t* _bookArenaBuf = nullptr;
    uint8_t* _scratchArenaBuf = nullptr;
    freeink::book::Arena _bookArena;
    freeink::book::Arena _scratchArena;
    
    // Book container
    freeink::book::BookCatalog _catalog;
    
    // Font setup
    BookFontManager::FontSetup _fontSetup;
    
    // Layout params
    freeink::book::LayoutParams _layoutParams;
    
    // Reading state
    State _state = State::LOADING;
    bool _shouldSleep = false;
    bool _needsRedraw = true;
    char _bookPath[128] = {};
    uint16_t _currentSpine = 0;
    uint32_t _currentPage = 0;
    uint32_t _totalPagesInChapter = 0;
    uint16_t _spineCount = 0;
    int _refreshCount = 0;
    uint32_t _lastActionTime = 0;
    
    // Bookmark
    struct Bookmark {
        static constexpr uint32_t MAGIC = 0x424B4E45;  // "ENKB"
        uint32_t magic = MAGIC;
        uint16_t spineIndex = 0;
        uint16_t padding = 0;
        uint32_t pageIndex = 0;
        uint32_t charOffset = 0;
    };
    Bookmark _bookmark;

    freeink::book::Page _currentPageData;
    size_t _pageMark = 0;
    
    // Framebuffer cache
    uint32_t _generationHash = 0;
    bool loadCachedFramebuffer();
    void saveCachedFramebuffer();
    void generateCoverThumbnail();
    void showLoadingIndicator();
    
    // PSRAM ring buffer cache (X4 Pro)
    struct FbCacheSlot {
        uint16_t spineIndex;
        uint32_t charStart;
        bool valid;
        uint8_t* framebuffer;  // 48000 bytes (800*480/8)
    };
    static constexpr size_t FB_SIZE = 800 * 528 / 8;  // 52800 bytes (fits X4: 48000 and X3: 52272)
    static constexpr int RING_CACHE_SLOTS = 4;
    FbCacheSlot _ringCache[RING_CACHE_SLOTS] = {};
    int _ringCacheNext = 0;
    bool _ringCacheEnabled = false;
    void initRingCache();
    bool loadFromRingCache();
    void saveToRingCache();

    // Helper to build cache file path
    void buildFbCachePath(char* out, size_t outLen) const;
    void buildStem(char* out, size_t outLen) const;

    // Internal methods
    bool openContainer();
    bool setupFonts();
    bool paginateChapter(uint16_t spineIndex, PageMatchMode mode = PageMatchMode::BY_INDEX, uint32_t targetVal = 0);
    void renderCurrentPage();
    void handleInput();
    void nextPage();
    void prevPage();
    void nextChapter();
    void prevChapter();
    void drawChrome();
    bool showConfirmExit();
    void saveProgress();
    void loadProgress();
    void exitToMenu();
};
