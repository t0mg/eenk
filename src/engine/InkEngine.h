#pragma once
#include "hal/IDisplay.h"
#include "hal/IInput.h"
#include "hal/IStorage.h"
#include "os/AppSettings.h"

#include "InkStoryManager.h"
#include "InkDisplayManager.h"
#include "InkInputHandler.h"
#include "StorySaveManager.h"

#include <cmath>

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
    
    StorySaveManager& getSaveManager() { return _saveManager; }
    const StorySaveManager& getSaveManager() const { return _saveManager; }
    IStorage& getStorage() { return _storage; }

    static bool parseCheckpointTag(const char* rawTag, std::string& outTitle);
    void triggerCheckpoint(const std::string& checkpointTitle);

    void update();
    bool isDone() const { return _state == State::DONE; }
    bool shouldSleep() const { return _shouldSleep; }
    
    void setShouldSleep(bool sleep) { _shouldSleep = sleep; }

    const std::deque<WrappedLine>& getHistory() const { return const_cast<InkDisplayManager&>(_displayManager).getHistory(); }
    void setHistory(const std::deque<WrappedLine>& history) { 
        _displayManager.clearHistory();
        for (const auto& l : history) _displayManager.addWrappedLine(l);
    }

    void applySettings(const struct AppSettings& settings);

    void setFrontlight(class IFrontlight* fl) { _frontlight = fl; }
    void setBatteryWidget(class BatteryWidget* bw) { _batteryWidget = bw; }

    int getImageHeight(const char* imagePath) const { return _storyManager.getImageHeight(imagePath); }

    enum class State {
        IDLE,
        RUNNING_TEXT,
        SHOWING_CHOICES,
        WAITING_INPUT,
        SAVE_STUB,
        STORY_ENDED,
        DONE,
    };
    State getState() const { return _state; }
    void setState(State state) { _state = state; }

    void incrementRefreshCount() { _refreshCount++; }
    void requestRedraw() { _needsRedraw = true; }
    void reloadSettings() { applySettings(AppSettings::load()); }

private:
    IDisplay&  _display;
    IInput&    _input;
    IStorage&  _storage;
    class IFrontlight* _frontlight = nullptr;
    class BatteryWidget* _batteryWidget = nullptr;
    struct AppSettings* _settingsObj = nullptr;

    AppSettings _settings = AppSettings::defaults(); 

    InkStoryManager _storyManager;
    InkDisplayManager _displayManager;
    InkInputHandler _inputHandler;
    StorySaveManager _saveManager;

    State _state = State::IDLE;
    bool _shouldSleep = false;
    bool _needsRedraw = false;
    int _refreshCount = 0;

    void tickRunningText();
};
