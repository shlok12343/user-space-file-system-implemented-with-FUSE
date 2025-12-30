#pragma once

extern "C" {
#include "blocks.h"
}

namespace nufs {

class Blocks {
public:
  static int bytesToBlocks(int bytes) {
    return bytes_to_blocks(bytes);
  }

  static void init(const char* image_path) {
    blocks_init(image_path);
  }

  static void freeImage() {
    blocks_free();
  }

  static void* getBlock(int bnum) {
    return blocks_get_block(bnum);
  }

  static void* blocksBitmap() {
    return get_blocks_bitmap();
  }

  static void* inodeBitmap() {
    return get_inode_bitmap();
  }

  static int allocBlock() {
    return alloc_block();
  }

  static void freeBlock(int bnum) {
    free_block(bnum);
  }
};

} // namespace nufs


