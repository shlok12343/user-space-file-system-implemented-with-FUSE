#ifndef INODE_H
#define INODE_H

#include <sys/types.h>

// Inode structure for the file system
typedef struct inode {
  int refs;       // reference count (for hard links)
  int mode;       // permission & type
  int size;       // in bytes
  int block;      // single block pointer for file data (or directory data)
  int uid;        // user id
  int gid;        // group id
  int mtime;      // modification time (seconds since epoch)
  int _reserved;  // padding to make struct size a power of 2
} inode_t;

// INODE_COUNT: Total number of inodes (256) available in the filesystem
//              Inode 0 is reserved for the root directory
//              Each inode is INODE_SIZE (32 bytes)
//              Inode table stored in blocks 1-2 (8KB total)
#define INODE_COUNT 256
#define INODE_SIZE sizeof(inode_t)

void print_inode(inode_t* node);
/** Get a pointer to the inode with the given number */
inode_t* get_inode(int inum);
/** Allocate a new inode and return its number, or -1 */
int alloc_inode();
/** Deallocate the inode with the given number */
void free_inode(int inum);
int grow_inode(inode_t* node, int size);
int shrink_inode(inode_t* node, int size);

#endif