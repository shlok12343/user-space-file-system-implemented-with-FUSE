#include "storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "helpers/bitmap/bitmap.h"
#include "helpers/blocks/blocks.h"

#define ROOT_INUM 0

int directory_lookup(inode_t* node, const char* name) {
  if (node->block == 0) return -1;
  dirent_t* entries = (dirent_t*)blocks_get_block(node->block);
  int num_entries = node->size / sizeof(dirent_t);
  for (int i = 0; i < num_entries; i++) {
    if (strcmp(entries[i].name, name) == 0) {
      return entries[i].inum;
    }
  }
  return -1;
}

int directory_put(inode_t* node, const char* name, int inum) {
  if (node->block == 0) {
    node->block = alloc_block();
    if (node->block < 0) return -ENOSPC;
  }
  dirent_t* entries = (dirent_t*)blocks_get_block(node->block);
  int num_entries = node->size / sizeof(dirent_t);
  if (num_entries >= DIRENTS_PER_BLOCK) return -ENOSPC;
  
  strncpy(entries[num_entries].name, name, DIR_NAME_LENGTH);
  entries[num_entries].inum = inum;
  node->size += sizeof(dirent_t);
  return 0;
}

int directory_delete(inode_t* node, const char* name) {
  if (node->block == 0) return -ENOENT;
  dirent_t* entries = (dirent_t*)blocks_get_block(node->block);
  int num_entries = node->size / sizeof(dirent_t);
  for (int i = 0; i < num_entries; i++) {
    if (strcmp(entries[i].name, name) == 0) {
      // Simple delete: swap with last entry
      entries[i] = entries[num_entries - 1];
      node->size -= sizeof(dirent_t);
      return 0;
    }
  }
  return -ENOENT;
}

/**
 * parse a path into parent directory inode and filename
 * returns parent inode number on success, negative error code on failure
 * 'name' parameter will point to the filename within path_copy
 */
void storage_init(const char* image_path) {
  blocks_init(image_path);

  void* ibm = get_inode_bitmap();
  if (!bitmap_get(ibm, ROOT_INUM)) {
    int root_inum = alloc_inode();
    if (root_inum != ROOT_INUM) {
      printf("Error: root inode allocation failed\n");
      return;
    }

    inode_t* root_inode = get_inode(ROOT_INUM);
    root_inode->mode = 040755;  // S_IFDIR | 0755
    root_inode->uid = getuid();
    root_inode->gid = getgid();
    root_inode->size = 0;
    root_inode->mtime = time(NULL);
    root_inode->block = 0;
    root_inode->refs = 1;
  }
}

int get_inode_by_path(const char* path) {
  if (path == NULL) {
    return -1;
  }

  if (strcmp(path, "/") == 0) {
    return ROOT_INUM;
  }
   // Just get the name after the first '/'
   const char* filename = path + 1; 

   // Look it up once in the root
   inode_t* root_node = get_inode(ROOT_INUM);
   return directory_lookup(root_node, filename);
}

int storage_stat(const char* path, struct stat* st) {
  int inum = get_inode_by_path(path);
  if (inum < 0) {
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  memset(st, 0, sizeof(struct stat));
  st->st_ino = inum;
  st->st_mode = node->mode;
  st->st_nlink = node->refs;
  st->st_uid = node->uid;
  st->st_gid = node->gid;
  st->st_size = node->size;
  st->st_mtime = node->mtime;
  st->st_atime = node->mtime;
  st->st_ctime = node->mtime;
  st->st_blocks = (node->size + 511) / 512;  // 512-byte blocks
  st->st_blksize = 4096;

  return 0;
}

char** storage_list(const char* path, int* count) {
  *count = 0;

  int inum = get_inode_by_path(path);
  if (inum < 0) {
    return NULL;
  }

  inode_t* node = get_inode(inum);
  if (!S_ISDIR(node->mode)) {
    return NULL;
  }

  if (node->block == 0) {
    return NULL;
  }

  dirent_t* entries = (dirent_t*)blocks_get_block(node->block);
  int num_entries = node->size / sizeof(dirent_t);

  int valid_count = 0;
  for (int i = 0; i < num_entries && i < DIRENTS_PER_BLOCK; i++) {
    if (entries[i].inum > 0) {
      valid_count++;
    }
  }

  if (valid_count == 0) {
    return NULL;
  }

  char** list = malloc(valid_count * sizeof(char*));
  int idx = 0;

  for (int i = 0; i < num_entries && i < DIRENTS_PER_BLOCK; i++) {
    if (entries[i].inum > 0) {
      list[idx] = malloc(DIR_NAME_LENGTH);
      strncpy(list[idx], entries[i].name, DIR_NAME_LENGTH);
      list[idx][DIR_NAME_LENGTH - 1] = '\0';
      idx++;
    }
  }

  *count = valid_count;
  return list;
}

void storage_list_free(char** list, int count) {
  if (list == NULL) {
    return;
  }

  for (int i = 0; i < count; i++) {
    free(list[i]);
  }
  free(list);
}

void storage_destroy() { blocks_free(); }
