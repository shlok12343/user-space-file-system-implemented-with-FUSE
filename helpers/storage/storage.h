#ifndef STORAGE_H
#define STORAGE_H

#include <sys/stat.h>

#include "helpers/storage/inode.h"

#define DIR_NAME_LENGTH 28

typedef struct dirent {
  char name[DIR_NAME_LENGTH];
  int inum;
} dirent_t;

#define DIRENTS_PER_BLOCK (4096 / sizeof(dirent_t))

/** Initialize the filesystem */
void storage_init(const char *image_path);
/** Get inode number by path */
int get_inode_by_path(const char* path);
/** Get file/directory attributes */
int storage_stat(const char* path, struct stat* st);
/** List directory contents - returns array of strings, count in *count parameter */
char** storage_list(const char* path, int* count);
/** Free list returned by storage_list */
void storage_list_free(char** list, int count);

// Directory functions (now part of storage)
int directory_lookup(inode_t* node, const char* name);
int directory_put(inode_t* node, const char* name, int inum);
int directory_delete(inode_t* node, const char* name);

/** Free all storage */
void storage_destroy();

#endif
