#include "directory.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../blocks/blocks.h"

/**
 * Look up a directory by name in a directory inode.
 * Returns inode number if found, -1 if no block allocated, -2 if not found
 */
int directory_lookup(inode_t* di, const char* name) {
  if (!di || !name) {
    return -1;
  }

  if (di->block == 0) {
    return -1;
  }

  dirent_t* entries = (dirent_t*)blocks_get_block(di->block);
  int num_entries = di->size / sizeof(dirent_t);

  for (int i = 0; i < num_entries && i < DIRENTS_PER_BLOCK; i++) {
    if (entries[i].inum > 0 && strcmp(entries[i].name, name) == 0) {
      return entries[i].inum;
    }
  }

  return -2;
}

int directory_put(inode_t* di, const char* name, int inum) {
  if (!di || !name || strlen(name) >= DIR_NAME_LENGTH) {
    return -1;
  }

  if (di->block == 0) {
    di->block = alloc_block();
    if (di->block < 0) {
      return -1;
    }
    di->size = 0;
  }

  dirent_t* entries = (dirent_t*)blocks_get_block(di->block);
  int num_entries = di->size / sizeof(dirent_t);

  if (directory_lookup(di, name) >= 0) {
    return -EEXIST;
  }

  for (int i = 0; i < DIRENTS_PER_BLOCK; i++) {
    if (entries[i].inum == 0) {
      strncpy(entries[i].name, name, DIR_NAME_LENGTH - 1);
      entries[i].name[DIR_NAME_LENGTH - 1] = '\0';
      entries[i].inum = inum;

      if (i >= num_entries) {
        di->size = (i + 1) * sizeof(dirent_t);
      }

      return 0;
    }
  }

  return -ENOSPC;
}

int directory_delete(inode_t* di, const char* name) {
  if (!di || !name) {
    return -1;
  }

  if (di->block == 0) {
    return -1;
  }

  dirent_t* entries = (dirent_t*)blocks_get_block(di->block);
  int num_entries = di->size / sizeof(dirent_t);

  for (int i = 0; i < num_entries && i < DIRENTS_PER_BLOCK; i++) {
    if (entries[i].inum > 0 && strcmp(entries[i].name, name) == 0) {
      entries[i].inum = 0;
      memset(entries[i].name, 0, DIR_NAME_LENGTH);

      if (i < num_entries - 1) {
        int last_idx = num_entries - 1;
        while (last_idx > i && entries[last_idx].inum == 0) {
          last_idx--;
        }
        if (last_idx > i && entries[last_idx].inum > 0) {
          entries[i] = entries[last_idx];
          entries[last_idx].inum = 0;
          memset(entries[last_idx].name, 0, DIR_NAME_LENGTH);
        }
      }

      int valid_count = 0;
      for (int j = 0; j < DIRENTS_PER_BLOCK; j++) {
        if (entries[j].inum > 0) {
          valid_count++;
        }
      }
      di->size = valid_count * sizeof(dirent_t);

      return 0;
    }
  }

  return -1;
}

void print_directory(inode_t* dd) {
  if (!dd) {
    printf("Empty directory\n");
    return;
  }

  if (dd->block == 0) {
    printf("Empty directory\n");
    return;
  }

  dirent_t* entries = (dirent_t*)blocks_get_block(dd->block);
  int num_entries = dd->size / sizeof(dirent_t);

  printf("Directory entries (%d):\n", num_entries);
  for (int i = 0; i < num_entries && i < DIRENTS_PER_BLOCK; i++) {
    if (entries[i].inum > 0) {
      printf("  %s -> inode %d\n", entries[i].name, entries[i].inum);
    }
  }
}
