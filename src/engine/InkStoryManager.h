#pragma once

#include "hal/IStorage.h"
#include "ui/StoryMetadata.h"
#include <story.h>
#include <runner.h>
#include <globals.h>
#include <snapshot.h>
#include <SDCardManager.h>
#include <map>
#include <string>

class InkStoryManager {
public:
    InkStoryManager(IStorage& storage);
    ~InkStoryManager();

#ifdef PLATFORM_NATIVE
    bool loadStory(const char* path, StoryMetadata& outMeta, std::string& outStoryBase, std::string& outStoryDir);
#else
    bool loadStory(const unsigned char* data, std::size_t size, const char* storyPath, StoryMetadata& outMeta, std::string& outStoryBase, std::string& outStoryDir);
#endif

    const unsigned char* createSnapshot(std::size_t* outLength);
    void freeSnapshot();
    bool loadSnapshot(const unsigned char* data, std::size_t length);

    ink::runtime::runner& runner() { return _runner; }
    ink::runtime::story* getStory() { return _story; }
    ink::runtime::globals& globals() { return _globals; }
    
    // Media access
    int getImageHeight(const char* imagePath) const;
    void loadMediaSidecar(bool hasMediaFlag, const char* storyPath);
    SdFile& getMediaFile() { return _mediaFile; }
    
    struct ImageMeta {
        uint32_t offset;
        uint32_t size;
        uint32_t width;
        uint32_t height;
    };
    bool getImageMeta(const char* imagePath, ImageMeta& outMeta) const;

private:
    IStorage& _storage;
    ink::runtime::story* _story = nullptr;
    ink::runtime::runner _runner;
    ink::runtime::snapshot* _currentSnapshot = nullptr;
    ink::runtime::globals _globals = nullptr;
    const unsigned char* _storyBuf = nullptr;

    std::map<uint32_t, ImageMeta> _mediaDict;
    SdFile _mediaFile;
};
