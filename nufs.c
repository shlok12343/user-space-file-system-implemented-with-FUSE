// based on cs3650 starter code

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "helpers/blocks/blocks.h"
#include "helpers/storage/inode.h"
#include "helpers/storage/storage.h"

// initialize a new inode with standard fields
static void init_inode(inode_t* inode, mode_t mode) {
  inode->mode = mode;
  inode->uid = getuid();
  inode->gid = getgid();
  inode->size = 0;
  inode->refs = 1;
  inode->mtime = time(NULL);
  inode->block = 0;
}

// implementation for: man 2 access
// Checks if a file exists.
int nufs_access(const char* path, int mask) {
  int rv = 0;

  int inum = get_inode_by_path(path);
  if (inum < 0) {
    rv = -ENOENT;
  }

  printf("access(%s, %04o) -> %d\n", path, mask, rv);
  return rv;
}

// Implementation for: man 2 stat
// *This is a crucial function.*

// st_dev should point to inode?

/** Gets an object's attributes (type, permissions, size, etc). */
int nufs_getattr(const char* path, struct stat* st) {
  int rv = storage_stat(path, st);
  printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", path, rv, st->st_mode,
         st->st_size);
  return rv;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info* fi) {
  struct stat st;
  int rv = 0;

  rv = nufs_getattr(path, &st);
  if (rv < 0) {
    printf("readdir(%s) -> %d\n", path, rv);
    return rv;
  }

  filler(buf, ".", &st, 0);

  
  if (strcmp(path, "/") != 0) {
    return -ENOTDIR;
  }

  int count = 0;
  char** list = storage_list(path, &count);

  for (int i = 0; i < count; i++) {
    char child_path[256];
    snprintf(child_path, sizeof(child_path), "/%s", list[i]);

    rv = nufs_getattr(child_path, &st);
    if (rv == 0) {
      filler(buf, list[i], &st, 0);
    }
  }

  storage_list_free(list, count);

  printf("readdir(%s) -> %d\n", path, 0);
  return 0;
}

// mknod makes a filesystem object like a file or directory(touch)
// called for: man 2 open, man 2 link
int nufs_mknod(const char* path, mode_t mode, dev_t rdev) {
  // In a flat filesystem, the parent is always root
  if (path[0] != '/' || strrchr(path, '/') != path) {
    return -EACCES; // Only allow files in root
  }
  
  const char* name = path + 1;
  inode_t* parent = get_inode(0);

  if (directory_lookup(parent, name) >= 0) {
    printf("mknod(%s, %04o) -> %d\n", path, mode, -EEXIST);
    return -EEXIST;
  }

  int new_inum = alloc_inode();
  if (new_inum < 0) {
    printf("mknod(%s, %04o) -> %d\n", path, mode, -ENOSPC);
    return -ENOSPC;
  }

  inode_t* new_inode = get_inode(new_inum);
  init_inode(new_inode, mode);

  int rv = directory_put(parent, name, new_inum);
  if (rv < 0) {
    free_inode(new_inum);
    printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
  }

  printf("mknod(%s, %04o) -> %d\n", path, mode, 0);
  return 0;
}

int nufs_mkdir(const char* path, mode_t mode) {
  return -EPERM;
}
// (rm)
int nufs_unlink(const char* path) {
  if (path[0] != '/' || strrchr(path, '/') != path) {
    return -ENOENT;
  }

  const char* name = path + 1;
  inode_t* parent = get_inode(0);
  
  int inum = directory_lookup(parent, name);
  if (inum < 0) {
    printf("unlink(%s) -> %d\n", path, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);
  
  if (S_ISDIR(node->mode)) {
    return -EISDIR;
  }

  node->refs--;
  
  if (node->refs == 0) {
    if (node->block != 0) {
      free_block(node->block);
    }
    free_inode(inum);
  }

  int rv = directory_delete(parent, name);
  printf("unlink(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_link(const char* from, const char* to) {
  return -EPERM;
}

int nufs_rmdir(const char* path) {
  return -EPERM;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char* from, const char* to) {
  // Only allow renaming within root
  if (from[0] != '/' || strrchr(from, '/') != from ||
      to[0] != '/' || strrchr(to, '/') != to) {
    return -EACCES;
  }

  const char* from_name = from + 1;
  const char* to_name = to + 1;
  inode_t* root = get_inode(0);

  int inum = directory_lookup(root, from_name);
  if (inum < 0) {
    return -ENOENT;
  }

  if (directory_lookup(root, to_name) >= 0) {
    return -EEXIST;
  }

  int rv = directory_delete(root, from_name);
  if (rv < 0) return rv;

  rv = directory_put(root, to_name, inum);
  return rv;
}

int nufs_chmod(const char* path, mode_t mode) {
  int rv = -1;
  printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

int nufs_truncate(const char* path, off_t size) {
  int inum = get_inode_by_path(path);
  if (inum < 0) {
    printf("truncate(%s, %ld bytes) -> %d\n", path, size, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  if (S_ISDIR(node->mode)) {
    printf("truncate(%s, %ld bytes) -> %d\n", path, size, -EISDIR);
    return -EISDIR;
  }

  if (size > 4096) {
    printf("truncate(%s, %ld bytes) -> %d\n", path, size, -EFBIG);
    return -EFBIG;
  }

  if (size > 0 && node->block == 0) {
    node->block = alloc_block();
    if (node->block < 0) {
      printf("truncate(%s, %ld bytes) -> %d\n", path, size, -ENOSPC);
      return -ENOSPC;
    }
  }
  
  if (size == 0 && node->block != 0) {
    free_block(node->block);
    node->block = 0;
  }

  node->size = size;
  node->mtime = time(NULL);

  printf("truncate(%s, %ld bytes) -> %d\n", path, size, 0);
  return 0;
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
int nufs_open(const char* path, struct fuse_file_info* fi) {
  int rv = 0;
  printf("open(%s) -> %d\n", path, rv);
  return rv;
}

// Actually read data
int nufs_read(const char* path, char* buf, size_t size, off_t offset,
              struct fuse_file_info* fi) {
  int inum = get_inode_by_path(path);
  if (inum < 0) {
    printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  if (S_ISDIR(node->mode)) {
    printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, -EISDIR);
    return -EISDIR;
  }

  if (offset >= node->size) {
    printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, 0);
    return 0;
  }
  int available = node->size - offset;
  int bytes_to_read = (size < available) ? size : available;

  if (node->block == 0) {
    printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, 0);
    return 0;
  }

  void* block_data = blocks_get_block(node->block);
  memcpy(buf, block_data + offset, bytes_to_read);
  printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset,
         bytes_to_read);
  return bytes_to_read;
}

int nufs_write(const char* path, const char* buf, size_t size, off_t offset,
               struct fuse_file_info* fi) {
  int inum = get_inode_by_path(path);
  if (inum < 0) {
    printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  if (S_ISDIR(node->mode)) {
    printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, -EISDIR);
    return -EISDIR;
  }

  int required_size = offset + size;
  
  if (required_size > 4096) {
    printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, -EFBIG);
    return -EFBIG;
  }

  if (node->block == 0) {
    node->block = alloc_block();
    if (node->block < 0) {
      printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, -ENOSPC);
      return -ENOSPC;
    }
  }

  void* block_data = blocks_get_block(node->block);
  memcpy(block_data + offset, buf, size);
  if (required_size > node->size) {
    node->size = required_size;
  }
  node->mtime = time(NULL);

  printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, (int)size);
  return size;
}

// Update the timestamps on a file or directory.
int nufs_utimens(const char* path, const struct timespec ts[2]) {
  int rv = -1;
  printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n", path, ts[0].tv_sec,
         ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
  return rv;
}

// Extended operations
int nufs_ioctl(const char* path, int cmd, void* arg, struct fuse_file_info* fi,
               unsigned int flags, void* data) {
  int rv = -1;
  printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
  return rv;
}

void nufs_init_ops(struct fuse_operations* ops) {
  memset(ops, 0, sizeof(struct fuse_operations));
  ops->access = nufs_access;
  ops->getattr = nufs_getattr;
  ops->readdir = nufs_readdir;
  ops->mknod = nufs_mknod;
  ops->mkdir = nufs_mkdir;
  ops->link = nufs_link;
  ops->unlink = nufs_unlink;
  ops->rmdir = nufs_rmdir;
  ops->rename = nufs_rename;
  ops->chmod = nufs_chmod;
  ops->truncate = nufs_truncate;
  ops->open = nufs_open;
  ops->read = nufs_read;
  ops->write = nufs_write;
  ops->utimens = nufs_utimens;
  ops->ioctl = nufs_ioctl;
};

struct fuse_operations nufs_ops;

int main(int argc, char* argv[]) {
  assert(argc > 2 && argc < 6);
  argc--;
  storage_init(argv[argc]);
  nufs_init_ops(&nufs_ops);
  return fuse_main(argc, argv, &nufs_ops, NULL);
}
