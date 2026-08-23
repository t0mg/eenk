#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "GfxRenderer.h"
#include "hal/IStorage.h"
#include "InkDisplayManager.h"

class InkStoryManager;

struct CheckpointEntry {
  std::string title; // "" for unnamed checkpoint, non-empty for named milestone
  std::vector<uint8_t> snapshotData;
  std::deque<WrappedLine> history;
};

class StorySaveManager {
public:
  static constexpr uint32_t MAGIC_ENK1 = 0x314B4E45; // "ENK1"
  static constexpr uint32_t MAGIC_ENK2 = 0x324B4E45; // "ENK2"

  StorySaveManager();
  ~StorySaveManager();

  void init(const std::string &saveFilePath, uint32_t storyHash);

  const std::string &getSaveFilePath() const { return _saveFilePath; }
  uint32_t getStoryHash() const { return _storyHash; }

  // Load save file from storage (supports ENK2 and backward-compat ENK1)
  bool loadSaveFile(IStorage &storage, InkStoryManager *storyMgr = nullptr);

  // Write ENK2 save file to storage
  bool writeSaveFile(IStorage &storage);

  // Main progress management
  bool hasMainProgress() const { return _hasMainProgress; }
  void saveMainProgress(const uint8_t *snapData, size_t snapLen,
                        const std::deque<WrappedLine> &history);
  bool restoreMainProgress(InkStoryManager &story, InkDisplayManager &display);

  // Checkpoint management (universal key rule: title is the key)
  void saveCheckpoint(const std::string &title, const uint8_t *snapData,
                      size_t snapLen, const std::deque<WrappedLine> &history);

  bool hasUnnamedCheckpoint() const;
  int getUnnamedCheckpointIndex() const;

  bool hasNamedCheckpoints() const;
  void getNamedCheckpoints(
      std::vector<std::pair<size_t, std::string>> &outNamed) const;

  bool restoreCheckpoint(size_t index, InkStoryManager &story,
                         InkDisplayManager &display);

  // Restart / wipe
  void clearAll(IStorage &storage);

  // Accessors
  const std::vector<CheckpointEntry> &getCheckpoints() const {
    return _checkpoints;
  }
  const std::vector<uint8_t> &getMainSnapshot() const {
    return _mainSnapshot;
  }
  const std::deque<WrappedLine> &getMainHistory() const { return _mainHistory; }

  // Serialization helpers (public for testing/utilities)
  static void serializeHistory(const std::deque<WrappedLine> &history,
                               std::vector<uint8_t> &out);
  static bool deserializeHistory(const uint8_t *&ptr, size_t &remaining,
                                 std::deque<WrappedLine> &out,
                                 InkStoryManager *storyMgr = nullptr);

private:
  std::string _saveFilePath;
  uint32_t _storyHash = 0;

  bool _hasMainProgress = false;
  std::vector<uint8_t> _mainSnapshot;
  std::deque<WrappedLine> _mainHistory;

  std::vector<CheckpointEntry> _checkpoints;
};
