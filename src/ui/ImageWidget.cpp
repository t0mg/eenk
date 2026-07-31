#include "ImageWidget.h"
#include <cstdio>
#include <cstdlib>

#define TAG "IMAGE_WIDGET"

void ImageWidget::draw(GfxRenderer &renderer, SdFile &file, uint32_t offset, uint32_t size, int imgW, int imgH, int x, int y, int maxWidth, int maxHeight) {
  if (!file) {
    printf("[%s] Invalid file object passed to draw\n", TAG);
    return;
  }
  
  if (!file.seek(offset)) {
    printf("[%s] Failed to seek to image offset %u\n", TAG, (unsigned)offset);
    return;
  }
  
  // Allocate buffer for 1-bit raw bitmap
  uint8_t* buffer = (uint8_t*)malloc(size);
  if (!buffer) {
    printf("[%s] Failed to allocate %u bytes for image\n", TAG, (unsigned)size);
    return;
  }
  
  if (file.read(buffer, size) != (int)size) {
    printf("[%s] Failed to read %u bytes\n", TAG, (unsigned)size);
    free(buffer);
    return;
  }
  
  // Align center if width is less than maxWidth
  int drawX = x;
  if (imgW < maxWidth) {
      drawX = x + (maxWidth - imgW) / 2;
  }
  
  int drawW = imgW < maxWidth ? imgW : maxWidth;
  int drawH = imgH < maxHeight ? imgH : maxHeight;
  
  renderer.drawImage(buffer, drawX, y, drawW, drawH);
  
  free(buffer);
}
