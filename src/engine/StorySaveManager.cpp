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

bool StorySaveManager::loadSaveFile(IStorage &storage,
                                    InkStoryManager *storyMgr) {
  if (_saveFilePath.empty() || !storage.fileExists(_saveFilePath.c_str())) {
    return false;
  }

  std::size_t fileSize = 0;
  const unsigned char *data =
      storage.readFileBinary(_saveFilePath.c_str(), &fileSize);
  if (!data || fileSize < 8) {
    if (data)
      storage.freeBuffer(data);
    return false;
  }

  const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data);
  size_t remaining = fileSize;

  uint32_t magic = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
  ptr += 4;
  remaining -= 4;

  uint32_t fileStoryHash =
      ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
  ptr += 4;
  remaining -= 4;

  if (_storyHash != 0 && fileStoryHash != _storyHash) {
    storage.freeBuffer(data);
    return false;
  }

  _hasMainProgress = false;
  _mainSnapshot.clear();
  _mainHistory.clear();
  _checkpoints.clear();

  if (magic == MAGIC_ENK1) {
    // ENK1 format: [snapSize(4), snapData(snapSize), historySize(2), lines...]
    if (remaining < 4) {
      storage.freeBuffer(data);
      return false;
    }
    uint32_t snapSize =
        ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
    ptr += 4;
    remaining -= 4;

    if (snapSize > 0 && snapSize <= remaining) {
      _mainSnapshot.assign(ptr, ptr + snapSize);
      ptr += snapSize;
      remaining -= snapSize;
      _hasMainProgress = true;

      deserializeHistory(ptr, remaining, _mainHistory, storyMgr);
    }
    storage.freeBuffer(data);
    return _hasMainProgress;
  } else if (magic == MAGIC_ENK2) {
    // Section 1: Main progress
    if (remaining < 1) {
      storage.freeBuffer(data);
      return false;
    }
    uint8_t hasMain = *ptr++;
    remaining -= 1;

    if (hasMain != 0) {
      if (remaining < 4) {
        storage.freeBuffer(data);
        return false;
      }
      uint32_t snapSize =
          ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
      ptr += 4;
      remaining -= 4;

      if (snapSize > remaining) {
        storage.freeBuffer(data);
        return false;
      }
      _mainSnapshot.assign(ptr, ptr + snapSize);
      ptr += snapSize;
      remaining -= snapSize;
      _hasMainProgress = true;

      if (!deserializeHistory(ptr, remaining, _mainHistory, storyMgr)) {
        storage.freeBuffer(data);
        return false;
      }
    }

    // Section 2: Unified checkpoints list
    if (remaining < 2) {
      storage.freeBuffer(data);
      return true; // Main progress loaded ok, empty checkpoints
    }
    uint16_t cpCount = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
    ptr += 2;
    remaining -= 2;

    for (uint16_t i = 0; i < cpCount; i++) {
      if (remaining < 2)
        break;
      uint16_t titleLen = ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8);
      ptr += 2;
      remaining -= 2;

      if (remaining < titleLen)
        break;
      std::string title(reinterpret_cast<const char *>(ptr), titleLen);
      ptr += titleLen;
      remaining -= titleLen;

      if (remaining < 4)
        break;
      uint32_t snapLen =
          ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
      ptr += 4;
      remaining -= 4;

      if (remaining < snapLen)
        break;
      std::vector<uint8_t> snap(ptr, ptr + snapLen);
      ptr += snapLen;
      remaining -= snapLen;

      std::deque<WrappedLine> history;
      if (!deserializeHistory(ptr, remaining, history, storyMgr)) {
        break;
      }

      _checkpoints.push_back({title, std::move(snap), std::move(history)});
    }

    storage.freeBuffer(data);
    return true;
  }

  storage.freeBuffer(data);
  return false;
}

bool StorySaveManager::writeSaveFile(IStorage &storage) {
  if (_saveFilePath.empty())
    return false;

  std::vector<uint8_t> out;

  // Header: magic (4), storyHash (4)
  uint32_t magic = MAGIC_ENK2;
  out.push_back(static_cast<uint8_t>(magic & 0xFF));
  out.push_back(static_cast<uint8_t>((magic >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((magic >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((magic >> 24) & 0xFF));

  out.push_back(static_cast<uint8_t>(_storyHash & 0xFF));
  out.push_back(static_cast<uint8_t>((_storyHash >> 8) & 0xFF));
  out.push_back(static_cast<uint8_t>((_storyHash >> 16) & 0xFF));
  out.push_back(static_cast<uint8_t>((_storyHash >> 24) & 0xFF));

  // Section 1: Main progress
  out.push_back(_hasMainProgress ? 1 : 0);
  if (_hasMainProgress) {
    uint32_t snapLen = static_cast<uint32_t>(_mainSnapshot.size());
    out.push_back(static_cast<uint8_t>(snapLen & 0xFF));
    out.push_back(static_cast<uint8_t>((snapLen >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((snapLen >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((snapLen >> 24) & 0xFF));
    out.insert(out.end(), _mainSnapshot.begin(), _mainSnapshot.end());

    serializeHistory(_mainHistory, out);
  }

  // Section 2: Unified Checkpoints List
  uint16_t cpCount = static_cast<uint16_t>(_checkpoints.size());
  out.push_back(static_cast<uint8_t>(cpCount & 0xFF));
  out.push_back(static_cast<uint8_t>((cpCount >> 8) & 0xFF));

  for (const auto &cp : _checkpoints) {
    uint16_t titleLen = static_cast<uint16_t>(cp.title.length());
    out.push_back(static_cast<uint8_t>(titleLen & 0xFF));
    out.push_back(static_cast<uint8_t>((titleLen >> 8) & 0xFF));
    if (titleLen > 0) {
      const uint8_t *tBytes =
          reinterpret_cast<const uint8_t *>(cp.title.c_str());
      out.insert(out.end(), tBytes, tBytes + titleLen);
    }

    uint32_t snapLen = static_cast<uint32_t>(cp.snapshotData.size());
    out.push_back(static_cast<uint8_t>(snapLen & 0xFF));
    out.push_back(static_cast<uint8_t>((snapLen >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((snapLen >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((snapLen >> 24) & 0xFF));
    out.insert(out.end(), cp.snapshotData.begin(), cp.snapshotData.end());

    serializeHistory(cp.history, out);
  }

  return storage.writeFileBinary(_saveFilePath.c_str(), out.data(),
                                 out.size());
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
  _checkpoints.push_back(
      {title, std::vector<uint8_t>(snapData, snapData + snapLen), history});
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
                                         InkDisplayManager &display) {
  if (index >= _checkpoints.size())
    return false;

  const auto &cp = _checkpoints[index];
  if (!story.loadSnapshot(cp.snapshotData.data(), cp.snapshotData.size()))
    return false;

  display.clearHistory();
  display.setScrollY(0);
  for (const auto &l : cp.history) {
    display.addWrappedLine(l);
  }

  // Update main progress to this restored checkpoint state
  _mainSnapshot = cp.snapshotData;
  _mainHistory = cp.history;
  _hasMainProgress = true;

  // Pruning: delete all checkpoint entries chronologically after index
  _checkpoints.erase(_checkpoints.begin() + index + 1, _checkpoints.end());

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
