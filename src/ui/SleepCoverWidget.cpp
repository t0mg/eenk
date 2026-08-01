#include "SleepCoverWidget.h"
#include <GfxRenderer.h>
#include <cstdio>
#include <cstring>
#include "os/BootManager.h"
#include <SDCardManager.h>
#include "ImageWidget.h"

void SleepCoverWidget::show(IDisplay &display, const char *msg,
                            const char *title) {
  auto renderer = display.getRenderer();
  if (renderer) {
    display.clear();

#if defined(PLATFORM_ESP32) || defined(PIO_UNIT_TESTING)
    static auto sleepcover_fnv1a_32 = [](const char *str) -> uint32_t {
      uint32_t hash = 2166136261u;
      while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 16777619u;
      }
      return hash;
    };

    bool drewCover = false;
    char storyPath[128] = {};
    if (BootManager::getStoryPath(storyPath, sizeof(storyPath)) && storyPath[0] != '\0') {
      char mediaPath[256];
      snprintf(mediaPath, sizeof(mediaPath), "%s", storyPath);
      char* dot = strrchr(mediaPath, '.');
      if (dot) {
        strcpy(dot, ".media");
        auto file = SDCardManager::getInstance().openFile(mediaPath);
        if (file) {
          uint32_t magic = 0;
          if (file.read((uint8_t*)&magic, 4) == 4 && magic == 0x4D4B4E45) { // "ENKM"
            uint32_t numEntries = 0;
            if (file.read((uint8_t*)&numEntries, 4) == 4) {
              uint32_t targetHash = sleepcover_fnv1a_32("@cover");
              for (uint32_t i = 0; i < numEntries; ++i) {
                uint32_t hash = 0, offset = 0, size = 0, w = 0, h = 0;
                if (file.read((uint8_t*)&hash, 4) != 4) break;
                if (file.read((uint8_t*)&offset, 4) != 4) break;
                if (file.read((uint8_t*)&size, 4) != 4) break;
                if (file.read((uint8_t*)&w, 4) != 4) break;
                if (file.read((uint8_t*)&h, 4) != 4) break;

                
                if (hash == targetHash) {
                  ImageWidget::draw(*renderer, file, offset, size, w, h, 0, 0, display.getWidth(), display.getHeight());
                  drewCover = true;
                  break;
                }
              }
            }
          }
          file.close();
        }
      }
    }

    if (!drewCover) {
      if (title && title[0] != '\0') {
        renderer->drawText(11, 50, 50, title);
      } else {
        renderer->drawText(11, 50, 50, "eenk");
      }
      renderer->drawText(10, 50, 90, msg);
    }
#endif

    display.fullRefresh();
  } else {
    printf("\nDevice sleeping...\n");
  }
}
