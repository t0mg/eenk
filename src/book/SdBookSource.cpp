#include "SdBookSource.h"
#include <cstring>

namespace eenk {
namespace book {

bool SdBookSource::open(const char* path) {
    close();

#ifdef PLATFORM_ESP32
    _file = SD_FS.open(path, FILE_READ);
    if (!_file) {
        return false;
    }
    _size = _file.size();
#else
    _file = std::fopen(path, "rb");
    if (!_file) {
        return false;
    }
    std::fseek(_file, 0, SEEK_END);
    _size = std::ftell(_file);
    std::fseek(_file, 0, SEEK_SET);
#endif
    return true;
}

void SdBookSource::close() {
#ifdef PLATFORM_ESP32
    if (_file) {
        _file.close();
    }
#else
    if (_file) {
        std::fclose(_file);
        _file = nullptr;
    }
#endif
    _size = 0;
}

int32_t SdBookSource::readAt(uint64_t offset, void* dst, uint32_t len) {
    if (offset >= _size) return 0;
    
    uint32_t toRead = len;
    if (offset + len > _size) {
        toRead = _size - offset;
    }

#ifdef PLATFORM_ESP32
    if (!_file) return -1;
    if (!_file.seek(offset)) return -1;
    return _file.read(static_cast<uint8_t*>(dst), toRead);
#else
    if (!_file) return -1;
    if (std::fseek(_file, offset, SEEK_SET) != 0) return -1;
    return std::fread(dst, 1, toRead, _file);
#endif
}

uint64_t SdBookSource::size() const {
    return _size;
}

} // namespace book
} // namespace eenk
