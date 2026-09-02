#include "StorySaveManager.h"
#include "InkDisplayManager.h"
#include "InkStoryManager.h"
#include <cstdio>
#include <cstring>

StorySaveManager::StorySaveManager() {}

StorySaveManager::~StorySaveManager() {}

void StorySaveManager::init(const std::string &saveFilePath,
                            uint32_t storyHash) {
  _saveFilePath = saveFilePath;
  _storyHash = storyHash;
  _hasMainProgress = false;
  _mainSnapshot.clear();
  _mainHistory.clear();
  _checkpoints.clear();
}

bool StorySaveManager::serializeHistory(const std::deque<WrappedLine> &history,
                                        IFileWriter &writer) {
  uint16_t historySize = static_cast<uint16_t>(history.size());
  uint8_t szBuf[2] = {static_cast<uint8_t>(historySize & 0xFF),
                      static_cast<uint8_t>((historySize >> 8) & 0xFF)};
  if (!writer.write(szBuf, 2))
    return false;

  for (const auto &line : history) {
    std::string text = line.block.getText();
    if (line.isImage) {
      text = "\x1B[IMG:" + line.imagePath + "]";
    }
    uint16_t len = static_cast<uint16_t>(text.length());
    uint8_t lenBuf[2] = {static_cast<uint8_t>(len & 0xFF),
                         static_cast<uint8_t>((len >> 8) & 0xFF)};
    if (!writer.write(lenBuf, 2))
      return false;

    if (len > 0) {
      if (!writer.write(text.data(), len))
        return false;
    }

    uint8_t flags = (line.isOld ? 1 : 0) | (line.endOfParagraph ? 2 : 0);
    if (!writer.write(&flags, 1))
      return false;
  }
  return true;
}

void StorySaveManager::serializeHistory(const std::deque<WrappedLine> &history,
                                        std::vector<uint8_t> &out) {
  uint16_t historySize = static_cast<uint16_t>(history.size());
  out.push_back(static_cast<uint8_t>(historySize & 0xFF));
  out.push_back(static_cast<uint8_t>((historySize >> 8) & 0xFF));

  for (const auto &line : history) {
    std::string text = line.block.getText();
    if (line.isImage) {
      text = "\x1B[IMG:" + line.imagePath + "]";
    }
    uint16_t len = static_cast<uint16_t>(text.length());
    out.push_back(static_cast<uint8_t>(len & 0xFF));
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));

    const uint8_t *textBytes =
        reinterpret_cast<const uint8_t *>(text.c_str());
    out.insert(out.end(), textBytes, textBytes + len);

    uint8_t flags = (line.isOld ? 1 : 0) | (line.endOfParagraph ? 2 : 0);
    out.push_back(flags);
  }
}

bool StorySaveManager::deserializeHistory(const uint8_t *&ptr,
                                          size_t &remaining,
                                          std::deque<WrappedLine> &out,
                                          InkStoryManager *storyMgr) {
  out.clear();
  if (remaining < 2)
    return false;

  uint16_t historySize = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
  ptr += 2;
  remaining -= 2;

  for (uint16_t i = 0; i < historySize; i++) {
    if (remaining < 2)
      return false;
    uint16_t lineLen = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
    ptr += 2;
    remaining -= 2;

    if (remaining < lineLen)
      return false;
    std::string str(reinterpret_cast<const char *>(ptr), lineLen);
    ptr += lineLen;
    remaining -= lineLen;

    if (remaining < 1)
      return false;
    uint8_t flags = *ptr++;
    remaining -= 1;

    bool isOld = (flags & 1) != 0;
    bool endOfParagraph = (flags & 2) != 0;
    bool isImage = false;
    std::string imagePath = "";

    if (str.length() > 7 && str.substr(0, 6) == "\x1B[IMG:") {
      isImage = true;
      imagePath = str.substr(6, str.length() - 7);
      str = "";
    }

    WrappedLine wl;
    wl.block = TextBlock(str);
    wl.isOld = isOld;
    wl.isImage = isImage;
    wl.imagePath = imagePath;
    wl.imageHeight = (isImage && storyMgr)
                         ? storyMgr->getImageHeight(imagePath.c_str())
                         : (isImage ? 280 : 0);
    wl.endOfParagraph = endOfParagraph;
    out.push_back(wl);
  }

  return true;
}

bool StorySaveManager::deserializeHistory(IFileReader &reader,
                                          std::deque<WrappedLine> &out,
                                          InkStoryManager *storyMgr) {
  out.clear();
  uint8_t szBuf[2];
  if (reader.read(szBuf, 2) != 2)
    return false;

  uint16_t historySize = szBuf[0] | (static_cast<uint16_t>(szBuf[1]) << 8);

  for (uint16_t i = 0; i < historySize; i++) {
    uint8_t lenBuf[2];
    if (reader.read(lenBuf, 2) != 2)
      return false;
    uint16_t lineLen = lenBuf[0] | (static_cast<uint16_t>(lenBuf[1]) << 8);

    std::string str;
    if (lineLen > 0) {
      str.resize(lineLen);
      if (reader.read(&str[0], lineLen) != lineLen)
        return false;
    }

    uint8_t flags = 0;
    if (reader.read(&flags, 1) != 1)
      return false;

    bool isOld = (flags & 1) != 0;
    bool endOfParagraph = (flags & 2) != 0;
    bool isImage = false;
    std::string imagePath = "";

    if (str.length() > 7 && str.substr(0, 6) == "\x1B[IMG:") {
      isImage = true;
      imagePath = str.substr(6, str.length() - 7);
      str = "";
    }

    WrappedLine wl;
    wl.block = TextBlock(str);
    wl.isOld = isOld;
    wl.isImage = isImage;
    wl.imagePath = imagePath;
    wl.imageHeight = (isImage && storyMgr)
                         ? storyMgr->getImageHeight(imagePath.c_str())
                         : (isImage ? 280 : 0);
    wl.endOfParagraph = endOfParagraph;
    out.push_back(wl);
  }

  return true;
}

bool StorySaveManager::loadSaveFile(IStorage &storage,
                                    InkStoryManager *storyMgr) {
  if (_saveFilePath.empty() || !storage.fileExists(_saveFilePath.c_str())) {
    return false;
  }

  _hasMainProgress = false;
  _mainSnapshot.clear();
  _mainHistory.clear();
  _checkpoints.clear();

  return storage.readStream(
      _saveFilePath.c_str(), [&](IFileReader &reader) -> bool {
        size_t fileSize = reader.size();
        if (fileSize < 8)
          return false;

        auto readU8 = [&](uint8_t &val) -> bool {
          return reader.read(&val, 1) == 1;
        };
        auto readU16 = [&](uint16_t &val) -> bool {
          uint8_t buf[2];
          if (reader.read(buf, 2) != 2)
            return false;
          val = buf[0] | (static_cast<uint16_t>(buf[1]) << 8);
          return true;
        };
        auto readU32 = [&](uint32_t &val) -> bool {
          uint8_t buf[4];
          if (reader.read(buf, 4) != 4)
            return false;
          val = buf[0] | (static_cast<uint32_t>(buf[1]) << 8) |
                (static_cast<uint32_t>(buf[2]) << 16) |
                (static_cast<uint32_t>(buf[3]) << 24);
          return true;
        };

        uint32_t magic = 0;
        if (!readU32(magic))
          return false;

        uint32_t fileStoryHash = 0;
        if (!readU32(fileStoryHash))
          return false;

        if (_storyHash != 0 && fileStoryHash != _storyHash) {
          printf("[StorySaveManager] Story hash mismatch! Expected 0x%08X, got "
                 "0x%08X\n",
                 (unsigned)_storyHash, (unsigned)fileStoryHash);
          return false;
        }

        if (magic == MAGIC_ENK1) {
          uint32_t snapSize = 0;
          if (!readU32(snapSize))
            return false;
          if (snapSize > 0) {
            _mainSnapshot.resize(snapSize);
            if (reader.read(_mainSnapshot.data(), snapSize) != snapSize)
              return false;
            _hasMainProgress = true;
            deserializeHistory(reader, _mainHistory, storyMgr);
          }
          return _hasMainProgress;
        } else if (magic == MAGIC_ENK2) {
          // Section 1: Main progress
          uint8_t hasMain = 0;
          if (!readU8(hasMain))
            return false;
          _hasMainProgress = (hasMain != 0);

          if (_hasMainProgress) {
            uint32_t snapSize = 0;
            if (!readU32(snapSize))
              return false;
            _mainSnapshot.resize(snapSize);
            if (snapSize > 0) {
              if (reader.read(_mainSnapshot.data(), snapSize) != snapSize)
                return false;
            }
            if (!deserializeHistory(reader, _mainHistory, storyMgr))
              return false;
          }

          // Section 2: Unified checkpoints list
          if (reader.tell() + 2 <= fileSize) {
            uint16_t cpCount = 0;
            if (readU16(cpCount)) {
              _checkpoints.reserve(cpCount);
              for (uint16_t i = 0; i < cpCount; i++) {
                uint16_t titleLen = 0;
                if (!readU16(titleLen))
                  break;
                std::string title;
                if (titleLen > 0) {
                  title.resize(titleLen);
                  if (reader.read(&title[0], titleLen) != titleLen)
                    break;
                }

                uint32_t snapLen = 0;
                if (!readU32(snapLen))
                  break;
                size_t snapOffset = reader.tell();

                // Skip snapshot payload in stream! Avoids allocating snapshot in heap.
                if (!reader.seek(snapOffset + snapLen))
                  break;

                std::deque<WrappedLine> history;
                if (!deserializeHistory(reader, history, storyMgr))
                  break;

                CheckpointEntry cp;
                cp.title = std::move(title);
                cp.fileOffset = snapOffset;
                cp.snapshotLen = snapLen;
                cp.history = std::move(history);
                _checkpoints.push_back(std::move(cp));
              }
            }
          }
          return true;
        }

        return false;
      });
}

bool StorySaveManager::writeSaveData(IFileWriter &writer,
                                     IFileReader *oldReader) {
  size_t currentOffset = 0;

  auto writeData = [&](const void *data, size_t size) -> bool {
    if (!writer.write(data, size))
      return false;
    currentOffset += size;
    return true;
  };

  auto writeU8 = [&](uint8_t val) -> bool { return writeData(&val, 1); };
  auto writeU16 = [&](uint16_t val) -> bool {
    uint8_t buf[2] = {static_cast<uint8_t>(val & 0xFF),
                      static_cast<uint8_t>((val >> 8) & 0xFF)};
    return writeData(buf, 2);
  };
  auto writeU32 = [&](uint32_t val) -> bool {
    uint8_t buf[4] = {static_cast<uint8_t>(val & 0xFF),
                      static_cast<uint8_t>((val >> 8) & 0xFF),
                      static_cast<uint8_t>((val >> 16) & 0xFF),
                      static_cast<uint8_t>((val >> 24) & 0xFF)};
    return writeData(buf, 4);
  };

  // Header: magic (4), storyHash (4)
  if (!writeU32(MAGIC_ENK2))
    return false;
  if (!writeU32(_storyHash))
    return false;

  // Section 1: Main progress
  if (!writeU8(_hasMainProgress ? 1 : 0))
    return false;
  if (_hasMainProgress) {
    uint32_t snapLen = static_cast<uint32_t>(_mainSnapshot.size());
    if (!writeU32(snapLen))
      return false;
    if (snapLen > 0) {
      if (!writeData(_mainSnapshot.data(), snapLen))
        return false;
    }
    uint16_t historySize = static_cast<uint16_t>(_mainHistory.size());
    if (!writeU16(historySize))
      return false;
    for (const auto &line : _mainHistory) {
      std::string text = line.block.getText();
      if (line.isImage) {
        text = "\x1B[IMG:" + line.imagePath + "]";
      }
      uint16_t len = static_cast<uint16_t>(text.length());
      if (!writeU16(len))
        return false;
      if (len > 0) {
        if (!writeData(text.data(), len))
          return false;
      }
      uint8_t flags = (line.isOld ? 1 : 0) | (line.endOfParagraph ? 2 : 0);
      if (!writeU8(flags))
        return false;
    }
  }

  // Section 2: Unified Checkpoints List
  uint16_t cpCount = static_cast<uint16_t>(_checkpoints.size());
  if (!writeU16(cpCount))
    return false;

  for (auto &cp : _checkpoints) {
    uint16_t titleLen = static_cast<uint16_t>(cp.title.length());
    if (!writeU16(titleLen))
      return false;
    if (titleLen > 0) {
      if (!writeData(cp.title.data(), titleLen))
        return false;
    }

    uint32_t snapLen = static_cast<uint32_t>(
        !cp.snapshotData.empty() ? cp.snapshotData.size() : cp.snapshotLen);
    if (!writeU32(snapLen))
      return false;

    size_t newOffset = currentOffset;

    if (!cp.snapshotData.empty()) {
      if (!writeData(cp.snapshotData.data(), snapLen))
        return false;
    } else if (oldReader && cp.fileOffset > 0 && snapLen > 0) {
      if (!oldReader->seek(cp.fileOffset))
        return false;
      uint8_t chunkBuf[512];
      size_t remaining = snapLen;
      while (remaining > 0) {
        size_t toRead = std::min(remaining, sizeof(chunkBuf));
        if (oldReader->read(chunkBuf, toRead) != toRead)
          return false;
        if (!writeData(chunkBuf, toRead))
          return false;
        remaining -= toRead;
      }
    }

    cp.fileOffset = newOffset;
    cp.snapshotLen = snapLen;

    uint16_t cpHistSize = static_cast<uint16_t>(cp.history.size());
    if (!writeU16(cpHistSize))
      return false;
    for (const auto &line : cp.history) {
      std::string text = line.block.getText();
      if (line.isImage) {
        text = "\x1B[IMG:" + line.imagePath + "]";
      }
      uint16_t len = static_cast<uint16_t>(text.length());
      if (!writeU16(len))
        return false;
      if (len > 0) {
        if (!writeData(text.data(), len))
          return false;
      }
      uint8_t flags = (line.isOld ? 1 : 0) | (line.endOfParagraph ? 2 : 0);
      if (!writeU8(flags))
        return false;
    }
  }

  return true;
}

bool StorySaveManager::writeSaveFile(IStorage &storage) {
  if (_saveFilePath.empty())
    return false;

  bool hasDiskCheckpoints = false;
  for (const auto &cp : _checkpoints) {
    if (cp.snapshotData.empty() && cp.fileOffset > 0 && cp.snapshotLen > 0) {
      hasDiskCheckpoints = true;
      break;
    }
  }

  if (hasDiskCheckpoints && storage.fileExists(_saveFilePath.c_str())) {
    std::string tmpPath = _saveFilePath + ".tmp";

    bool ok = storage.readStream(
        _saveFilePath.c_str(), [&](IFileReader &oldReader) -> bool {
          return storage.writeStream(
              tmpPath.c_str(), [&](IFileWriter &writer) -> bool {
                return writeSaveData(writer, &oldReader);
              });
        });

    if (!ok) {
      storage.deleteFile(tmpPath.c_str());
      return false;
    }

    storage.deleteFile(_saveFilePath.c_str());
    if (!storage.renameFile(tmpPath.c_str(), _saveFilePath.c_str())) {
      return false;
    }

    for (auto &cp : _checkpoints) {
      cp.snapshotData.clear();
      cp.snapshotData.shrink_to_fit();
    }
    return true;
  }

  bool ok = storage.writeStream(
      _saveFilePath.c_str(),
      [&](IFileWriter &writer) -> bool { return writeSaveData(writer, nullptr); });

  if (ok) {
    for (auto &cp : _checkpoints) {
      cp.snapshotData.clear();
      cp.snapshotData.shrink_to_fit();
    }
  }
  return ok;
}

void StorySaveManager::saveMainProgress(
    const uint8_t *snapData, size_t snapLen,
    const std::deque<WrappedLine> &history) {
  if (snapData && snapLen > 0) {
    _mainSnapshot.assign(snapData, snapData + snapLen);
    _mainHistory = history;
    _hasMainProgress = true;
  }
}

bool StorySaveManager::restoreMainProgress(InkStoryManager &story,
                                           InkDisplayManager &display) {
  if (!_hasMainProgress || _mainSnapshot.empty())
    return false;
  if (!story.loadSnapshot(_mainSnapshot.data(), _mainSnapshot.size()))
    return false;

  display.clearHistory();
  display.setScrollY(0);
  for (const auto &l : _mainHistory) {
    display.addWrappedLine(l);
  }
  return true;
}

void StorySaveManager::saveCheckpoint(
    const std::string &title, const uint8_t *snapData, size_t snapLen,
    const std::deque<WrappedLine> &history) {
  if (!snapData || snapLen == 0)
    return;

  // 1. Deduplicate: remove any existing entry matching this key
  for (auto it = _checkpoints.begin(); it != _checkpoints.end(); ++it) {
    if (it->title == title) {
      _checkpoints.erase(it);
      break;
    }
  }

  // 2. Append new checkpoint snapshot at the end (latest chronological tail)
  // Named checkpoints (chapters/milestones) do not need display history, saving significant RAM
  if (!title.empty()) {
    _checkpoints.push_back(
        {title, std::vector<uint8_t>(snapData, snapData + snapLen), {}});
  } else {
    _checkpoints.push_back(
        {title, std::vector<uint8_t>(snapData, snapData + snapLen), history});
  }
}

bool StorySaveManager::hasUnnamedCheckpoint() const {
  return getUnnamedCheckpointIndex() >= 0;
}

int StorySaveManager::getUnnamedCheckpointIndex() const {
  for (int i = static_cast<int>(_checkpoints.size()) - 1; i >= 0; --i) {
    if (_checkpoints[i].title.empty()) {
      return i;
    }
  }
  return -1;
}

bool StorySaveManager::hasNamedCheckpoints() const {
  for (const auto &cp : _checkpoints) {
    if (!cp.title.empty())
      return true;
  }
  return false;
}

void StorySaveManager::getNamedCheckpoints(
    std::vector<std::pair<size_t, std::string>> &outNamed) const {
  outNamed.clear();
  for (size_t i = 0; i < _checkpoints.size(); ++i) {
    if (!_checkpoints[i].title.empty()) {
      outNamed.push_back({i, _checkpoints[i].title});
    }
  }
}

bool StorySaveManager::restoreCheckpoint(size_t index, InkStoryManager &story,
                                         InkDisplayManager &display,
                                         IStorage *storage) {
  if (index >= _checkpoints.size())
    return false;

  const auto &cp = _checkpoints[index];

  if (!cp.snapshotData.empty()) {
    if (!story.loadSnapshot(cp.snapshotData.data(), cp.snapshotData.size()))
      return false;
    _mainSnapshot = cp.snapshotData;
  } else if (cp.fileOffset > 0 && cp.snapshotLen > 0 && storage) {
    bool loaded = false;
    storage->readStream(
        _saveFilePath.c_str(), [&](IFileReader &reader) -> bool {
          if (!reader.seek(cp.fileOffset))
            return false;
          std::vector<uint8_t> snapBuf(cp.snapshotLen);
          if (reader.read(snapBuf.data(), cp.snapshotLen) != cp.snapshotLen)
            return false;
          if (!story.loadSnapshot(snapBuf.data(), cp.snapshotLen))
            return false;
          _mainSnapshot = std::move(snapBuf);
          loaded = true;
          return true;
        });
    if (!loaded)
      return false;
  } else {
    return false;
  }

  display.clearHistory();
  display.setScrollY(0);
  for (const auto &l : cp.history) {
    display.addWrappedLine(l);
  }

  // Update main progress to this restored checkpoint state
  _mainHistory = cp.history;
  _hasMainProgress = true;

  // Pruning: delete all checkpoint entries chronologically after index
  if (index + 1 < _checkpoints.size()) {
    _checkpoints.erase(_checkpoints.begin() + index + 1, _checkpoints.end());
    _checkpoints.shrink_to_fit();
  }

  return true;
}

void StorySaveManager::clearAll(IStorage &storage) {
  _hasMainProgress = false;
  _mainSnapshot.clear();
  _mainHistory.clear();
  _checkpoints.clear();
  if (!_saveFilePath.empty() && storage.fileExists(_saveFilePath.c_str())) {
    storage.deleteFile(_saveFilePath.c_str());
  }
}
