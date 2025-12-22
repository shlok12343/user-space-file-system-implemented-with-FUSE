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
#include "helpers/directory/directory.h"

#define ROOT_INUM 0

/**
 * parse a path into parent directory inode and filename
 * returns parent inode number on success, negative error code on failure
 * 'name' parameter will point to the filename within path_copy
 */
int parse_path_parent(const char* path, char** path_copy_out, char** name_out) {
  *path_copy_out = malloc(strlen(path) + 1);
  strcpy(*path_copy_out, path);

  char* last_slash = strrchr(*path_copy_out, '/');
  if (last_slash == NULL) {
    free(*path_copy_out);
    return -EINVAL;
  }

  *name_out = last_slash + 1;
  if (strlen(*name_out) == 0 || strlen(*name_out) >= DIR_NAME_LENGTH) {
    free(*path_copy_out);
    return -ENAMETOOLONG;
  }

  int parent_inum;
  if (last_slash == *path_copy_out) {
    parent_inum = ROOT_INUM;
  } else {
    *last_slash = '\0';
    parent_inum = get_inode_by_path(*path_copy_out);
    *last_slash = '/';
  }

  if (parent_inum < 0) {
    free(*path_copy_out);
    return -ENOENT;
  }

  inode_t* parent = get_inode(parent_inum);
  if (!S_ISDIR(parent->mode)) {
    free(*path_copy_out);
    return -ENOTDIR;
  }

  return parent_inum;
}

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

  char* path_copy = malloc(strlen(path) + 1);
  strcpy(path_copy, path);

  int current_inum = ROOT_INUM;

  char* token = strtok(path_copy, "/");
  while (token != NULL) {
    if (strlen(token) == 0) {
      token = strtok(NULL, "/");
      continue;
    }

    inode_t* current_inode = get_inode(current_inum);
    if (!S_ISDIR(current_inode->mode)) {
      free(path_copy);
      return -1;
    }

    int next_inum = directory_lookup(current_inode, token);
    if (next_inum < 0) {
      free(path_copy);
      return -1;
    }

    current_inum = next_inum;
    token = strtok(NULL, "/");
  }

  free(path_copy);
  return current_inum;
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
