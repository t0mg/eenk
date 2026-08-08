#include "InkStoryManager.h"
#include <cstdio>
#include <cstring>

// FNV-1a hash function
static constexpr uint32_t fnv1a_32(const char* s, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint8_t>(s[i]);
        hash *= 16777619u;
    }
    return hash;
}

static inline uint32_t fnv1a_32(const char* s) {
    uint32_t hash = 2166136261u;
    while (*s) {
        hash ^= static_cast<uint8_t>(*s++);
        hash *= 16777619u;
    }
    return hash;
}

InkStoryManager::InkStoryManager(IStorage& storage) : _storage(storage) {
}

InkStoryManager::~InkStoryManager() {
    freeSnapshot();
    if (_story) {
        delete _story;
        _story = nullptr;
    }
    if (_storyBuf) {
        _storage.freeBuffer(_storyBuf);
        _storyBuf = nullptr;
    }
    if (_mediaFile) {
        _mediaFile.close();
    }
}

#ifdef PLATFORM_NATIVE
bool InkStoryManager::loadStory(const char* path, StoryMetadata& outMeta, std::string& outStoryBase, std::string& outStoryDir) {
    std::size_t size = 0;
    _storyBuf = _storage.readFileBinary(path, &size);
    if (!_storyBuf || size == 0) {
        fprintf(stderr, "[InkStoryManager] Failed to read: %s\n", path);
        return false;
    }

    const unsigned char* dataToLoad = _storyBuf;
    std::size_t sizeToLoad = size;

    memset(&outMeta, 0, sizeof(outMeta));
    if (sizeToLoad >= 128 && StoryMetadata::hasHeader(_storyBuf, sizeToLoad)) {
        StoryMetadata::parse(_storyBuf, sizeToLoad, &outMeta);
        dataToLoad += 128;
        sizeToLoad -= 128;
    }

    _story = ink::runtime::story::from_binary(dataToLoad, sizeToLoad, false);
    if (!_story) {
        fprintf(stderr, "[InkStoryManager] story::from_binary() failed\n");
        _storage.freeBuffer(_storyBuf);
        _storyBuf = nullptr;
        return false;
    }

    _globals = _story->new_globals();
    _runner = _story->new_runner(_globals);

    printf("[InkStoryManager] Story loaded — %zu bytes\n", size);

    char storyBase[64] = {};
    const char* lastSlash = strrchr(path, '/');
#ifdef _WIN32
    const char* lastBackslash = strrchr(path, '\\');
    if (lastBackslash > lastSlash)
        lastSlash = lastBackslash;
#endif
    const char* fname = lastSlash ? lastSlash + 1 : path;
    strncpy(storyBase, fname, sizeof(storyBase) - 1);
    char* dot = strrchr(storyBase, '.');
    if (dot)
        *dot = '\0';
    outStoryBase = storyBase;

    char storyDir[512] = {};
    if (lastSlash) {
        size_t dirLen = lastSlash - path;
        if (dirLen < sizeof(storyDir)) {
            strncpy(storyDir, path, dirLen);
            storyDir[dirLen] = '\0';
        }
    }
    outStoryDir = storyDir;

    return true;
}
#else
bool InkStoryManager::loadStory(const unsigned char* data, std::size_t size, const char* storyPath, StoryMetadata& outMeta, std::string& outStoryBase, std::string& outStoryDir) {
    const unsigned char* dataToLoad = data;
    std::size_t sizeToLoad = size;

    memset(&outMeta, 0, sizeof(outMeta));
    if (sizeToLoad >= 128 && StoryMetadata::hasHeader(dataToLoad, sizeToLoad)) {
        StoryMetadata::parse(dataToLoad, sizeToLoad, &outMeta);
        dataToLoad += 128;
        sizeToLoad -= 128;
    }

    _story = ink::runtime::story::from_binary(dataToLoad, sizeToLoad, false);
    if (!_story) {
        fprintf(stderr, "[InkStoryManager] story::from_binary() failed (mmap)\n");
        return false;
    }

    _globals = _story->new_globals();
    _runner = _story->new_runner(_globals);

    printf("[InkStoryManager] Story loaded from memory — %zu bytes\n", size);

    char storyBase[64] = {};
    char storyDir[512] = {};
    if (storyPath && storyPath[0]) {
        const char* lastSlash = strrchr(storyPath, '/');
#ifdef _WIN32
        const char* lastBackslash = strrchr(storyPath, '\\');
        if (lastBackslash > lastSlash)
            lastSlash = lastBackslash;
#endif
        if (lastSlash) {
            size_t dirLen = lastSlash - storyPath;
            if (dirLen < sizeof(storyDir)) {
                strncpy(storyDir, storyPath, dirLen);
                storyDir[dirLen] = '\0';
            }
        }
        const char* fname = lastSlash ? lastSlash + 1 : storyPath;
        strncpy(storyBase, fname, sizeof(storyBase) - 1);
        char* dot = strrchr(storyBase, '.');
        if (dot)
            *dot = '\0';
    }
    outStoryBase = storyBase;
    outStoryDir = storyDir;

    return true;
}
#endif

const unsigned char* InkStoryManager::createSnapshot(std::size_t* outLength) {
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

void InkStoryManager::freeSnapshot() {
    if (_currentSnapshot) {
        delete _currentSnapshot;
        _currentSnapshot = nullptr;
    }
}

bool InkStoryManager::loadSnapshot(const unsigned char* data, std::size_t length) {
    if (!_story)
        return false;

    ink::runtime::snapshot* snap = ink::runtime::snapshot::from_binary(data, length, false);
    if (!snap)
        return false;

    _globals = _story->new_globals_from_snapshot(*snap);
    if (!_globals) {
        printf("[InkStoryManager] Failed to load globals from snapshot\n");
        delete snap;
        return false;
    }

    _runner = _story->new_runner_from_snapshot(*snap, _globals);
    if (!_runner) {
        printf("[InkStoryManager] Failed to load runner from snapshot\n");
        _globals = nullptr;
        delete snap;
        return false;
    }

    delete snap;
    return true;
}

void InkStoryManager::loadMediaSidecar(bool hasMediaFlag, const char* storyPath) {
    _mediaDict.clear();
    if (!hasMediaFlag || !storyPath || !storyPath[0])
        return;
    if (_mediaFile)
        _mediaFile.close();

    char mediaPath[512] = {};
    snprintf(mediaPath, sizeof(mediaPath), "%s", storyPath);
    char* dot = strrchr(mediaPath, '.');
    if (dot) {
        strcpy(dot, ".media");
    } else {
        strcat(mediaPath, ".media");
    }

    _mediaFile = SDCardManager::getInstance().openFile(mediaPath);
    if (!_mediaFile) {
        printf("[InkStoryManager] Media sidecar expected but not found at %s\n", mediaPath);
        return;
    }

    uint8_t magic[4];
    if (_mediaFile.read(magic, 4) != 4 || memcmp(magic, "ENKM", 4) != 0) {
        printf("[InkStoryManager] Media sidecar has invalid magic\n");
        return;
    }

    uint32_t count = 0;
    if (_mediaFile.read(reinterpret_cast<uint8_t*>(&count), 4) != 4)
        return;

    for (uint32_t i = 0; i < count; i++) {
        uint8_t buf[20];
        if (_mediaFile.read(buf, 20) != 20)
            break;

        uint32_t hash = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        ImageMeta meta;
        meta.offset = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
        meta.size = buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24);
        meta.width = buf[12] | (buf[13] << 8) | (buf[14] << 16) | (buf[15] << 24);
        meta.height = buf[16] | (buf[17] << 8) | (buf[18] << 16) | (buf[19] << 24);
        _mediaDict[hash] = meta;
    }

    printf("[InkStoryManager] Loaded %zu media dictionary entries\n", _mediaDict.size());
}

bool InkStoryManager::getImageMeta(const char* imagePath, ImageMeta& outMeta) const {
    uint32_t hash = fnv1a_32(imagePath);
    auto it = _mediaDict.find(hash);
    if (it != _mediaDict.end()) {
        outMeta = it->second;
        return true;
    }
    return false;
}

int InkStoryManager::getImageHeight(const char* imagePath) const {
    ImageMeta meta;
    if (getImageMeta(imagePath, meta)) {
        return meta.height;
    }
    return 280; // fallback
}
