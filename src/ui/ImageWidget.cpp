#include "ImageWidget.h"
#include <cstdio>
#include <cstdlib>

#define TAG "IMAGE_WIDGET"

void ImageWidget::draw(GfxRenderer &renderer, SdFile &file, uint32_t offset, uint32_t size, int imgW, int imgH, int x, int y, int maxWidth, int maxHeight) {
  if (!file) {
    printf("[%s] Invalid file object passed to draw\n", TAG);
    return;
  }

  if (imgW <= 0 || imgH <= 0 || size == 0) {
    return;
  }

  // Align center if width is less than maxWidth
  int drawX = x;
  if (imgW < maxWidth) {
    drawX = x + (maxWidth - imgW) / 2;
  }

  int drawW = imgW < maxWidth ? imgW : maxWidth;
  int drawH = imgH < maxHeight ? imgH : maxHeight;

  if (drawW <= 0 || drawH <= 0) {
    return;
  }

  // Clip vertical coordinates to screen boundaries
  const int screenH = renderer.getScreenHeight();
  if (y >= screenH) {
    return; // Entire image is below screen
  }

  const int fileRowBytes = (imgW + 7) / 8;
  const int drawRowBytes = (drawW + 7) / 8;

  if (y < 0) {
    int skipRows = -y;
    if (skipRows >= drawH) {
      return; // Entire image is above screen
    }
    offset += (uint32_t)skipRows * fileRowBytes;
    drawH -= skipRows;
    y = 0;
  }

  if (y + drawH > screenH) {
    drawH = screenH - y;
  }

  if (drawH <= 0) {
    return;
  }

  if (!file.seek(offset)) {
    printf("[%s] Failed to seek to image offset %u\n", TAG, (unsigned)offset);
    return;
  }

  // Target small chunk buffer (e.g. 2048 bytes) to avoid needing large contiguous heap
  constexpr size_t TARGET_CHUNK_SIZE = 2048;
  int rowsPerChunk = TARGET_CHUNK_SIZE / fileRowBytes;
  if (rowsPerChunk < 1) rowsPerChunk = 1;
  if (rowsPerChunk > drawH) rowsPerChunk = drawH;

  size_t rowStride = (drawW == imgW ? fileRowBytes : drawRowBytes);
  size_t bufferSize = rowsPerChunk * rowStride;

  uint8_t stackFallback[256];
  uint8_t *buffer = (uint8_t*)malloc(bufferSize);
  bool allocatedOnHeap = (buffer != nullptr);
  if (!buffer) {
    if (sizeof(stackFallback) >= rowStride) {
      buffer = stackFallback;
      rowsPerChunk = sizeof(stackFallback) / rowStride;
      bufferSize = rowsPerChunk * rowStride;
    } else {
      printf("[%s] Failed to allocate %u bytes for image chunk\n", TAG, (unsigned)bufferSize);
      return;
    }
  }

  int rowsDrawn = 0;
  while (rowsDrawn < drawH) {
    int rowsThisChunk = rowsPerChunk;
    if (rowsDrawn + rowsThisChunk > drawH) {
      rowsThisChunk = drawH - rowsDrawn;
    }

    if (drawW == imgW) {
      int bytesToRead = rowsThisChunk * fileRowBytes;
      if (file.read(buffer, bytesToRead) != bytesToRead) {
        printf("[%s] Failed to read chunk at row %d\n", TAG, rowsDrawn);
        break;
      }
      renderer.drawImage(buffer, drawX, y + rowsDrawn, drawW, rowsThisChunk);
    } else {
      bool readOk = true;
      for (int r = 0; r < rowsThisChunk; ++r) {
        uint32_t rowFileOffset = offset + (uint32_t)(rowsDrawn + r) * fileRowBytes;
        if (!file.seek(rowFileOffset)) {
          readOk = false;
          break;
        }
        if (file.read(&buffer[r * drawRowBytes], drawRowBytes) != drawRowBytes) {
          readOk = false;
          break;
        }
      }
      if (!readOk) {
        printf("[%s] Failed to read cropped chunk at row %d\n", TAG, rowsDrawn);
        break;
      }
      renderer.drawImage(buffer, drawX, y + rowsDrawn, drawW, rowsThisChunk);
    }

    rowsDrawn += rowsThisChunk;
  }

  if (allocatedOnHeap) {
    free(buffer);
  }
}
