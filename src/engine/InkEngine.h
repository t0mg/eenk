#pragma once
#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "hal/IStorage.h"
#include "os/AppSettings.h"

class StreamingEpdFontFamily; // forward declaration — owned by InkEngine when SD font is active
struct StoryMetadata;          // forward declaration

// <cmath> must be included at global scope before InkCPP headers so GCC 15's
// specfun.h special math functions (sph_bessel, etc.) are not injected into
// ink::runtime namespace.
#include <cmath>

// InkCPP types (story_ptr smart handles)
#include <story.h>
#include <runner.h>
#include <globals.h>

#include <cstddef>
#include <vector>
#include <deque>
#include <string>
#include <map>
#include <TextBlock.h>

// FNV-1a hash function
constexpr uint32_t fnv1a_32(const char* s, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint8_t>(s[i]);
        hash *= 16777619u;
    }
    return hash;
}

inline uint32_t fnv1a_32(const char* s) {
    uint32_t hash = 2166136261u;
    while (*s) {
        hash ^= static_cast<uint8_t>(*s++);
        hash *= 16777619u;
    }
    return hash;
}

class InkEngine
{
public:
    InkEngine(IDisplay& display, IInput& input, IStorage& storage);
    ~InkEngine();

#ifdef PLATFORM_NATIVE
    bool loadStory(const char* path);
#else
    bool loadStory(const unsigned char* data, std::size_t size, const char* storyPath = nullptr);
#endif
    
    // Save/Load system
    const unsigned char* createSnapshot(std::size_t* outLength);
    void freeSnapshot();
    bool loadSnapshot(const unsigned char* data, std::size_t length);
    
    void update();
    bool isDone() const { return _state == State::DONE; }
    bool shouldSleep() const { return _shouldSleep; }

    struct WrappedLine {
        TextBlock block;
        bool isOld = false;
        bool isImage = false;
        std::string imagePath = "";
        int imageHeight = 0;
    };
    const std::deque<WrappedLine>& getHistory() const { return _wrappedLines; }
    void setHistory(const std::deque<WrappedLine>& history) { _wrappedLines = history; }

    void applySettings(const struct AppSettings& settings);

    int getImageHeight(const char* imagePath) const;

private:
    IDisplay&  _display;
    IInput&    _input;
    IStorage&  _storage;
    struct AppSettings* _settingsObj = nullptr;

    AppSettings _settings = AppSettings::defaults(); // cached settings for FontResolver
    StreamingEpdFontFamily* _streamingFamily = nullptr; // non-null when an SD .epdfont family is active

    ink::runtime::story*  _story   = nullptr;
    ink::runtime::runner  _runner;
    ink::runtime::snapshot* _currentSnapshot = nullptr;
    ink::runtime::globals _globals = nullptr;
    const unsigned char*  _storyBuf = nullptr;

    std::string _storyDir;

    std::deque<WrappedLine> _wrappedLines;
    int _scrollY = 0;
    int _maxScrollY = 0;
    int _refreshCount = 0;

    struct ImageMeta {
        uint32_t offset;
        uint32_t size;
        uint32_t width;
        uint32_t height;
    };
    std::map<uint32_t, ImageMeta> _mediaDict;
    void loadMediaSidecar(bool hasMediaFlag);

    static constexpr int MAX_CHOICES = 8;
    char  _choiceText[MAX_CHOICES][128] = {};
    std::vector<TextBlock> _wrappedChoices[MAX_CHOICES];
    int   _numChoices     = 0;
    int   _selectedChoice = 0;

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
    bool _shouldSleep = false;

    void tickRunningText();
    void tickWaitingInput();
    void tickStoryEnded();
    void redraw();
    void collectChoices();
    void drawNarrativeArea();
    void drawChoiceArea();
    int  getChoicesHeight(class GfxRenderer* renderer) const;

    // FontResolver: parse header hint, select and apply the narrative font family.
    void _resolveAndApplyFont(const StoryMetadata& meta, const char* storyBase, const char* storyDir = nullptr);
};
