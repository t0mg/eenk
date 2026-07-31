#pragma once
#include <SDCardManager.h>
#include <GfxRenderer.h>
#include <cstdint>

class ImageWidget {
public:
  static void draw(GfxRenderer &renderer, SdFile &file, uint32_t offset, uint32_t size, int imgW, int imgH, int x, int y, int maxWidth, int maxHeight);
};
