#ifndef DIRECTORY_H
#define DIRECTORY_H

#define DIR_NAME_LENGTH 28

#include "../storage/inode.h"

typedef struct dirent {
  char name[DIR_NAME_LENGTH];
  int inum;
} dirent_t;

// Number of directory entries per block
#define DIRENTS_PER_BLOCK (4096 / sizeof(dirent_t))

/**
 * Look up a file/directory by name in a directory inode. 
 * Returns inode number if found, -1 if not found
 */
int directory_lookup(inode_t* i, const char* name);

/**
 * Add an entry to a directory.
 *
 * @param i Pointer to directory inode
 * @param name Name of the entry
 * @param inum Inode number of the entry
 * @return 0 on success, -1 on failure
 */
int directory_put(inode_t* i, const char* name, int inum);

/**
 * Remove an entry from a directory.
 *
 * @param di Pointer to directory inode
 * @param name Name of the entry to remove
 * @return 0 on success, -1 on failure
 */
int directory_delete(inode_t* di, const char* name);

/** Print directory contents (for debugging). */
void print_directory(inode_t* dd);

#endif
