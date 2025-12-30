#pragma once

extern "C" {
#include "inode.h"
}

namespace nufs {

class Inode {
public:
  static inode_t* get(int inum) {
    return get_inode(inum);
  }

  static int alloc() {
    return alloc_inode();
  }

  static void freeInode(int inum) {
    free_inode(inum);
  }

  static int grow(inode_t* node, int size) {
    return grow_inode(node, size);
  }

  static int shrink(inode_t* node, int size) {
    return shrink_inode(node, size);
  }
};

} // namespace nufs


