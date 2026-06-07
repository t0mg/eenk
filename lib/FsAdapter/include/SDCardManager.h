#pragma once
#include <SdFat.h>

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#include <LittleFS.h>

class SDCardManager {
public:
    static SDCardManager& getInstance() { static SDCardManager instance; return instance; }
    FsFile openFile(const char* path) { 
        return FsFile(LittleFS.open(path, "r")); 
    }
    FsFile open(const char* path, int mode) { 
        return FsFile(LittleFS.open(path, "r")); 
    }
    bool openFileForRead(const char* type, const char* path, FsFile& file) { 
        File f = LittleFS.open(path, "r");
        if (f) { file = FsFile(f); return true; }
        return false;
    }
    bool openFileForWrite(const char* type, const char* path, FsFile& file) { 
        File f = LittleFS.open(path, "w");
        if (f) { file = FsFile(f); return true; }
        return false;
    }
};

#define SdMan SDCardManager::getInstance()
