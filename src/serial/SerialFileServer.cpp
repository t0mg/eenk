#ifdef PLATFORM_ESP32

#include "SerialFileServer.h"
#include <rom/crc.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <algorithm>

SerialFileServer::SerialFileServer() {
}

SerialFileServer::~SerialFileServer() {
}

bool SerialFileServer::poll() {
    if (!Serial.available()) {
        return false;
    }

    char buf[128];
    if (readLine(buf, sizeof(buf), 50)) {
        if (strcmp(buf, SYNC_MAGIC) == 0) {
            uint64_t freeBytes = 0;
            uint64_t total = SD_FS.totalBytes();
            if (total == 0) total = SD_FS.cardSize();
            if (total > 0) {
                freeBytes = total - SD_FS.usedBytes();
            }
            sendLine("OK EENK %s %llu", PROTOCOL_VERSION, (unsigned long long)freeBytes);
            if (onConnect) {
                onConnect();
            }
            runCommandLoop();
            return true;
        }
    }

    // Not sync, drain/ignore the data (it's probably debug noise from the host)
    while (Serial.available()) {
        Serial.read();
    }
    
    return false;
}

void SerialFileServer::runCommandLoop() {
    Serial.println("[SerialFileServer] Entering command mode");
    char line[512];

    while (true) {
        if (!Serial.available()) {
            delay(5);
            continue;
        }

        if (!readLine(line, sizeof(line), CMD_TIMEOUT_MS)) {
            continue;
        }

        // Parse command: find first space
        char* space = strchr(line, ' ');
        const char* cmd = line;
        const char* args = nullptr;
        if (space) {
            *space = '\0';
            args = space + 1;
        }

        if (strcmp(cmd, "DISCONNECT") == 0) {
            sendLine("OK BYE");
            break;
        } else if (strcmp(cmd, "LIST") == 0 && args) {
            handleList(args);
        } else if (strcmp(cmd, "DELETE") == 0 && args) {
            handleDelete(args);
        } else if (strcmp(cmd, "UPLOAD") == 0 && args) {
            const char* space2 = strchr(args, ' ');
            if (space2) {
                char path[256];
                size_t pathLen = space2 - args;
                if (pathLen >= sizeof(path)) pathLen = sizeof(path) - 1;
                strncpy(path, args, pathLen);
                path[pathLen] = '\0';
                uint32_t size = strtoul(space2 + 1, nullptr, 10);
                handleUpload(path, size);
            } else {
                sendError("INVALID_CMD", "UPLOAD missing size");
            }
        } else if (strcmp(cmd, "DOWNLOAD") == 0 && args) {
            handleDownload(args);
        } else if (strcmp(cmd, "MKDIR") == 0 && args) {
            handleMkdir(args);
        } else if (strcmp(cmd, "INFO") == 0) {
            handleInfo();
        } else if (strcmp(cmd, "EENK_SYNC") == 0) {
            uint32_t freeBytes = SD_FS.totalBytes() - SD_FS.usedBytes();
            sendLine("OK %lu", (unsigned long)freeBytes);
        } else {
            sendError("INVALID_CMD", cmd);
        }
    }
    
    Serial.println("[SerialFileServer] Exited command mode");
}

void SerialFileServer::handleList(const char* path) {
    File dir = SD_FS.open(path);
    if (!dir || !dir.isDirectory()) {
        sendError("NOT_FOUND", path);
        if (dir) dir.close();
        return;
    }

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        const char* name = entry.name();
        
        // Extract base name
        const char* lastSlash = strrchr(name, '/');
        const char* baseName = lastSlash ? lastSlash + 1 : name;
        
        if (baseName[0] != '.' && strcasecmp(baseName, "System Volume Information") != 0) {
            const char* type = entry.isDirectory() ? "D" : "F";
            size_t size = entry.isDirectory() ? 0 : entry.size();
            sendLine("FILE %s %u %s", type, (unsigned)size, baseName);
        }
        entry.close();
    }
    dir.close();
    sendLine("END");
}

void SerialFileServer::handleDelete(const char* path) {
    if (!SD_FS.exists(path)) {
        sendLine("ERR_NOT_FOUND File does not exist");
        return;
    }
    
    File f = SD_FS.open(path);
    bool isDir = f ? f.isDirectory() : false;
    if (f) f.close();

    bool success = isDir ? removeDirectoryRecursively(path) : SD_FS.remove(path);
    if (success) {
        sendLine("OK DELETE");
    } else {
        sendError("IO_ERROR");
    }
}

void SerialFileServer::handleUpload(const char* path, uint32_t totalSize) {
    // Create parent directories if needed
    char parentPath[256];
    strncpy(parentPath, path, sizeof(parentPath) - 1);
    parentPath[sizeof(parentPath) - 1] = '\0';
    char* lastSlash = strrchr(parentPath, '/');
    if (lastSlash && lastSlash != parentPath) {
        *lastSlash = '\0';
        makeDirectory(parentPath);
    }

    // Check if card has enough space
    if (totalSize > (SD_FS.totalBytes() - SD_FS.usedBytes())) {
        sendLine("ERR_NOSPACE Not enough storage space");
        return;
    }

    File f = SD_FS.open(path, FILE_WRITE);
    if (!f) {
        sendError("IO_ERROR", "Failed to open for write");
        return;
    }

    sendLine("OK READY");

    uint32_t bytesReceivedTotal = 0;
    uint32_t runningCrc = 0;
    char line[64];
    uint8_t buf[CHUNK_SIZE];
    
    uint32_t oldTimeout = Serial.getTimeout();
    Serial.setTimeout(CMD_TIMEOUT_MS);

    while (bytesReceivedTotal < totalSize) {
        if (!readLine(line, sizeof(line), CMD_TIMEOUT_MS)) {
            sendError("TIMEOUT", "Waiting for CHUNK line");
            f.close();
            SD_FS.remove(path);
            Serial.setTimeout(oldTimeout);
            return;
        }

        uint32_t chunkSize = 0;
        if (sscanf(line, "CHUNK %u", &chunkSize) != 1) {
            sendError("INVALID_CMD", "Expected CHUNK <size>");
            f.close();
            SD_FS.remove(path);
            Serial.setTimeout(oldTimeout);
            return;
        }

        if (chunkSize > sizeof(buf)) {
            sendError("INVALID_CMD", "Chunk too large");
            f.close();
            SD_FS.remove(path);
            Serial.setTimeout(oldTimeout);
            return;
        }

        // Read raw bytes
        size_t readCount = Serial.readBytes(buf, chunkSize);
        if (readCount != chunkSize) {
            sendError("TIMEOUT", "Chunk read incomplete");
            f.close();
            SD_FS.remove(path);
            Serial.setTimeout(oldTimeout);
            return;
        }

        size_t written = f.write(buf, chunkSize);
        if (written != chunkSize) {
            sendError("IO_ERROR", "Disk write failed");
            f.close();
            SD_FS.remove(path);
            Serial.setTimeout(oldTimeout);
            return;
        }

        runningCrc = crc32_le(runningCrc, buf, chunkSize);
        bytesReceivedTotal += chunkSize;

        sendLine("OK CHUNK %u", bytesReceivedTotal);
    }

    if (!readLine(line, sizeof(line), CMD_TIMEOUT_MS)) {
        sendError("TIMEOUT", "Waiting for END");
        f.close();
        SD_FS.remove(path);
        Serial.setTimeout(oldTimeout);
        return;
    }

    uint32_t expectedCrc = 0;
    if (sscanf(line, "END %x", &expectedCrc) != 1) {
        sendError("INVALID_CMD", "Expected END <crc_hex>");
        f.close();
        SD_FS.remove(path);
        Serial.setTimeout(oldTimeout);
        return;
    }

    f.close();
    Serial.setTimeout(oldTimeout);

    if (runningCrc == expectedCrc) {
        sendLine("OK UPLOAD %08x", runningCrc);
    } else {
        sendLine("ERR CRC_MISMATCH %08x %08x", expectedCrc, runningCrc);
        SD_FS.remove(path);
    }
}

void SerialFileServer::handleDownload(const char* path) {
    File f = SD_FS.open(path, FILE_READ);
    if (!f) {
        sendError("NOT_FOUND", path);
        return;
    }
    if (f.isDirectory()) {
        sendError("IO_ERROR", "Path is a directory");
        f.close();
        return;
    }

    uint32_t totalSize = f.size();
    sendLine("OK DOWNLOAD %u", totalSize);

    uint8_t buf[CHUNK_SIZE];
    uint32_t bytesSentTotal = 0;
    uint32_t runningCrc = 0;

    while (bytesSentTotal < totalSize) {
        uint32_t chunkSize = std::min((uint32_t)CHUNK_SIZE, totalSize - bytesSentTotal);
        size_t readCount = f.read(buf, chunkSize);
        if (readCount != chunkSize) {
            sendError("IO_ERROR", "File read failed");
            f.close();
            return;
        }

        sendLine("CHUNK %u", chunkSize);
        Serial.write(buf, chunkSize);

        runningCrc = crc32_le(runningCrc, buf, chunkSize);
        bytesSentTotal += chunkSize;
    }

    f.close();
    sendLine("END %08x", runningCrc);
}

void SerialFileServer::handleMkdir(const char* path) {
    if (makeDirectory(path)) {
        sendLine("OK MKDIR");
    } else {
        sendError("IO_ERROR", "Mkdir failed");
    }
}

void SerialFileServer::handleInfo() {
    uint64_t total = SD_FS.totalBytes();
    if (total == 0) total = SD_FS.cardSize();
    uint64_t used = SD_FS.usedBytes();
    uint64_t free = total > used ? total - used : 0;
    
    // If total is STILL 0 but we know we're here, just fake a size so the UI doesn't think it's absent
    if (total == 0) total = 1000000000ULL;
    
    sendLine("OK INFO %llu %llu %llu", (unsigned long long)total, (unsigned long long)used, (unsigned long long)free);
}

bool SerialFileServer::makeDirectory(const char* path) {
    if (SD_FS.exists(path)) return true;
    
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!SD_FS.exists(tmp)) {
                SD_FS.mkdir(tmp);
            }
            *p = '/';
        }
    }
    return SD_FS.mkdir(tmp);
}

bool SerialFileServer::removeDirectoryRecursively(const char* path) {
    File dir = SD_FS.open(path);
    if (!dir || !dir.isDirectory()) {
        return false;
    }
    
    bool success = true;
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        
        char fullPath[256];
        const char* name = entry.name();
        
        // Some ESP32 SD library versions return the full path in entry.name(), others just the basename.
        if (name[0] == '/') {
            snprintf(fullPath, sizeof(fullPath), "%s", name);
        } else {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", path, name);
        }
        
        bool isDir = entry.isDirectory();
        entry.close();
        
        if (isDir) {
            if (!removeDirectoryRecursively(fullPath)) success = false;
        } else {
            if (!SD_FS.remove(fullPath)) success = false;
        }
    }
    dir.close();
    
    if (success) {
        return SD_FS.rmdir(path);
    }
    return false;
}

bool SerialFileServer::readLine(char* buf, size_t maxLen, uint32_t timeoutMs) {
    uint32_t start = millis();
    size_t idx = 0;
    
    while (millis() - start < timeoutMs) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') {
                buf[idx] = '\0';
                // Strip trailing \r if present
                if (idx > 0 && buf[idx-1] == '\r') {
                    buf[idx-1] = '\0';
                }
                return true;
            }
            if (idx < maxLen - 1) {
                buf[idx++] = c;
            }
        } else {
            // Short delay to yield CPU if waiting
            delay(1);
        }
    }
    return false;
}

void SerialFileServer::sendLine(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
    Serial.print("\n");
}

void SerialFileServer::sendError(const char* code, const char* detail) {
    if (detail) {
        sendLine("ERR %s %s", code, detail);
    } else {
        sendLine("ERR %s", code);
    }
}

#endif // PLATFORM_ESP32
