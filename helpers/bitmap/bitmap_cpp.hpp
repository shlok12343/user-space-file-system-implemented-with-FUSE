#pragma once

extern "C" {
#include "helpers/bitmap/bitmap.h"
}

namespace nufs {

class Bitmap {
public:
  static int get(void* bm, int i) {
    return bitmap_get(bm, i);
  }

  static void put(void* bm, int i, int v) {
    bitmap_put(bm, i, v);
  }

  static void print(void* bm, int size) {
    bitmap_print(bm, size);
  }
};

} // namespace nufs


