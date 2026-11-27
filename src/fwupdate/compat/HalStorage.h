#pragma once
// Minimal HalStorage/HalFile subset over the Arduino SD driver.
//
// Exists for ONE purpose: letting ../FirmwareFlasher.cpp stay a byte-identical
// copy of crosspoint-reader's. CrossPoint's real HalStorage (lib/hal) is a
// mutex-guarded multi-client filesystem layer; eenk mounts the SD card globally
// (EspSdStorage::begin) and passes Arduino FS File handles around directly
// (see Library::scanSD), so the flasher only needs:
//   Storage.openFileForRead(module, path, file)
//   HalFile::{read, fileSize, close, operator bool}
// If a future flasher re-sync needs more, extend HERE, never fork the flasher.
//
// Only compiled into [env:esp32c3_locked] (see platformio.ini build_src_filter
// exclusions + -Isrc/fwupdate/compat on the locked env).

#include <Arduino.h>

#include "HalTypes.h"

#include <string>

class HalFile {
 public:
  HalFile() = default;
  ~HalFile() { close(); }
  HalFile(HalFile&& o) noexcept : _f(o._f), _open(o._open) { o._open = false; }
  HalFile& operator=(HalFile&& o) noexcept {
    if (this != &o) {
      close();
      _f = o._f;
      _open = o._open;
      o._open = false;
    }
    return *this;
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  size_t fileSize() { return _open ? _f.size() : 0; }
  int read(void* buf, size_t count) {
    if (!_open) return -1;
    return static_cast<int>(_f.read(static_cast<uint8_t*>(buf), count));
  }
  bool close() {
    if (_open) {
      _f.close();
      _open = false;
    }
    return true;
  }
  bool isOpen() const { return _open; }
  operator bool() const { return _open && static_cast<bool>(_f); }

 private:
  friend class HalStorage;
  File _f;
  bool _open = false;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  bool openFileForRead(const char* module, const char* path, HalFile& file) {
    (void)module;
    // Caller must have mounted the SD card (EspSdStorage::begin / MENU branch /
    // recovery dispatch in main.cpp). SD.begin() is idempotent, so a second
    // mount attempt elsewhere is harmless.
    file.close();
    file._f = SD_FS.open(path, FILE_READ);
    file._open = static_cast<bool>(file._f);
    return file._open;
  }
  bool openFileForRead(const char* module, const std::string& path, HalFile& file) {
    return openFileForRead(module, path.c_str(), file);
  }

 private:
  HalStorage() = default;
};

// Same call shape as CrossPoint (`Storage.openFileForRead(...)`) but a plain
// reference, not a macro, so no identifier is hijacked in includers.
static HalStorage& Storage = HalStorage::getInstance();
