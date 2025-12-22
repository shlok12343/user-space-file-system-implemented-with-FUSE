#include "inode.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../blocks/blocks.h"
#include "../bitmap/bitmap.h"

// Get a pointer to the inode with the given number.
inode_t* get_inode(int inum) {
  // inode table starts at block 1
  // each block can hold BLOCK_SIZE / 32 = 128 inodes
  // so we have 2 blocks for 256 inodes

  uint8_t* inode_table = blocks_get_block(1);
  return (inode_t*)(inode_table + (inum * 32));
}

void print_inode(inode_t* node) {
  if (node == NULL) {
    printf("inode is NULL\n");
    return;
  }

  printf("inode: refs=%d mode=%o size=%d block=%d uid=%d gid=%d mtime=%d\n",
         node->refs, node->mode, node->size, node->block, node->uid, node->gid,
         node->mtime);
}

// assuming files are max 4KB, only need one block, so not needed
// copied from initial inode.h
int grow_inode(inode_t* node, int size) {
  if (node->block == 0) {
    node->block = alloc_block();
    if (node->block < 0) {
      return -1;
    }
  }

  node->size = size;
  return 0;
}

// see grow_inode
int shrink_inode(inode_t* node, int size) {
  if (size == 0 && node->block != 0) {
    free_block(node->block);
    node->block = 0;
  }

  node->size = size;
  return 0;
}

/** Deallocate the inode with the given number */
void free_inode(int inum) {
  printf("+ free_inode(%d)\n", inum);
  void *bitmap = get_inode_bitmap();
  bitmap_put(bitmap, inum, 0);
}

/** Allocate a new inode and return its number, or -1 */
int alloc_inode() {
  void *ibm = get_inode_bitmap();

  for (int ii = 0; ii < 256; ++ii) {
    if (!bitmap_get(ibm, ii)) {
      bitmap_put(ibm, ii, 1);
      printf("+ alloc_inode() -> %d\n", ii);
      
      // Note: avoid calling get_inode here to prevent circular dependency
      // instead calculate the pointer directly
      uint8_t *inode_table = blocks_get_block(1);
      void *inode_ptr = (void *)(inode_table + (ii * 32));
      memset(inode_ptr, 0, 32); // sizeof(inode_t) = 32 bytes
      
      return ii;
    }
  }

  return -1;
}
