#pragma once

#ifdef PLATFORM_ESP32

#include <Arduino.h>
#include <SD.h>
#include <cstdint>
#include <cstddef>
#include <functional>

class SerialFileServer {
public:
    SerialFileServer();
    ~SerialFileServer();

    // Non-blocking poll. Called every iteration of the main loop.
    // Returns true if the command loop was entered and exited, false otherwise.
    bool poll();

    std::function<void()> onConnect;

private:
    void runCommandLoop();

    void handleList(const char* path);
    void handleDelete(const char* path);
    void handleUpload(const char* path, uint32_t totalSize);
    void handleDownload(const char* path);
    void handleMkdir(const char* path);
    void handleInfo();

    bool readLine(char* buf, size_t maxLen, uint32_t timeoutMs);
    void sendLine(const char* fmt, ...);
    void sendError(const char* code, const char* detail = nullptr);

    // Recursive mkdir helper
    bool makeDirectory(const char* path);
    // Recursive rm helper
    bool removeDirectoryRecursively(const char* path);

    static constexpr int CHUNK_SIZE = 4096;
    static constexpr int CMD_TIMEOUT_MS = 10000;
    static constexpr const char* PROTOCOL_VERSION = "1";
    static constexpr const char* SYNC_MAGIC = "EENK_SYNC";
};

#endif // PLATFORM_ESP32
