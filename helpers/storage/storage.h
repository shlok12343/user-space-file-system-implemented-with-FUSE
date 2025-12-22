#ifndef STORAGE_H
#define STORAGE_H

#include <sys/stat.h>

#include "helpers/storage/inode.h"

/** Initialize the filesystem */
void storage_init(const char *image_path);
/** Get inode number by path */
int get_inode_by_path(const char* path);
/** Parse path into parent directory and filename */
int parse_path_parent(const char* path, char** path_copy_out, char** name_out);
/** Get file/directory attributes */
int storage_stat(const char* path, struct stat* st);
/** List directory contents - returns array of strings, count in *count parameter */
char** storage_list(const char* path, int* count);
/** Free list returned by storage_list */
void storage_list_free(char** list, int count);

/** Free all storage */
void storage_destroy();

#endif
