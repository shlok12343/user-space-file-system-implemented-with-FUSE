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
#include "helpers/directory/directory.h"
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
    char parent_path[256];
    strncpy(parent_path, path, sizeof(parent_path));
    char* last_slash = strrchr(parent_path, '/');
    if (last_slash != NULL) {
      if (last_slash == parent_path) {
        strcpy(parent_path, "/");
      } else {
        *last_slash = '\0';
      }
      rv = nufs_getattr(parent_path, &st);
      if (rv == 0) {
        filler(buf, "..", &st, 0);
      }
    }
  }

  int count = 0;
  char** list = storage_list(path, &count);

  for (int i = 0; i < count; i++) {
    char child_path[256];
    if (strcmp(path, "/") == 0) {
      snprintf(child_path, sizeof(child_path), "/%s", list[i]);
    } else {
      snprintf(child_path, sizeof(child_path), "%s/%s", path, list[i]);
    }

    rv = nufs_getattr(child_path, &st);
    if (rv == 0) {
      filler(buf, list[i], &st, 0);
    }
  }

  storage_list_free(list, count);

  printf("readdir(%s) -> %d\n", path, 0);
  return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
int nufs_mknod(const char* path, mode_t mode, dev_t rdev) {
  char* path_copy;
  char* name;

  int parent_inum = parse_path_parent(path, &path_copy, &name);
  if (parent_inum < 0) {
    printf("mknod(%s, %04o) -> %d\n", path, mode, parent_inum);
    return parent_inum;
  }

  inode_t* parent = get_inode(parent_inum);

  if (directory_lookup(parent, name) >= 0) {
    free(path_copy);
    printf("mknod(%s, %04o) -> %d\n", path, mode, -EEXIST);
    return -EEXIST;
  }

  int new_inum = alloc_inode();
  if (new_inum < 0) {
    free(path_copy);
    printf("mknod(%s, %04o) -> %d\n", path, mode, -ENOSPC);
    return -ENOSPC;
  }

  inode_t* new_inode = get_inode(new_inum);
  init_inode(new_inode, mode);

  int rv = directory_put(parent, name, new_inum);
  if (rv < 0) {
    free_inode(new_inum);
    free(path_copy);
    printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
  }

  free(path_copy);
  printf("mknod(%s, %04o) -> %d\n", path, mode, 0);
  return 0;
}

int nufs_mkdir(const char* path, mode_t mode) {
  char* path_copy;
  char* name;
  int parent_inum = parse_path_parent(path, &path_copy, &name);
  if (parent_inum < 0) {
    printf("mkdir(%s) -> %d\n", path, parent_inum);
    return parent_inum;
  }

  inode_t* parent = get_inode(parent_inum);
  int new_inum = alloc_inode();
  if (new_inum < 0) {
    free(path_copy);
    printf("mkdir(%s) -> %d\n", path, -ENOSPC);
    return -ENOSPC;
  }
  inode_t* new_dir_inode = get_inode(new_inum);
  init_inode(new_dir_inode, S_IFDIR | mode);
  new_dir_inode->block = alloc_block();
  if (new_dir_inode->block < 0) {
    free_inode(new_inum);
    free(path_copy);
    printf("mkdir(%s) -> %d\n", path, -ENOSPC);
    return -ENOSPC;
  }

  int rv = directory_put(parent, name, new_inum);
  if (rv < 0) {
    free_block(new_dir_inode->block);
    free_inode(new_inum);
  }

  free(path_copy);
  printf("mkdir(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_unlink(const char* path) {
  char* path_copy;
  char* name;
  
  int parent_inum = parse_path_parent(path, &path_copy, &name);
  if (parent_inum < 0) {
    printf("unlink(%s) -> %d\n", path, parent_inum);
    return parent_inum;
  }

  inode_t* parent = get_inode(parent_inum);
  
  int inum = directory_lookup(parent, name);
  if (inum < 0) {
    free(path_copy);
    printf("unlink(%s) -> %d\n", path, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);
  
  if (S_ISDIR(node->mode)) {
    free(path_copy);
    printf("unlink(%s) -> %d\n", path, -EISDIR);
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
  free(path_copy);
  
  printf("unlink(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_link(const char* from, const char* to) {
  int rv = -1;
  printf("link(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_rmdir(const char* path) {
  int rv = -1;
  printf("rmdir(%s) -> %d\n", path, rv);

  char* path_copy;
  char* name;

  int parent_inum = parse_path_parent(path, &path_copy, &name); //supplies the parent inum and name of dir to remove

  inode_t* parent = get_inode(parent_inum);

  int inum = directory_lookup(parent, name); //find the inum of the dir we want to remove

  inode_t* dir_inode = get_inode(inum); //FINALLY the inode of the dir we want to remove

  //if the dir is not empty, prevent removal
  if (dir_inode->size > 0) {
    free(path_copy);
    printf("rmdir(%s) -> %d\n", path, -ENOTEMPTY);
    return -ENOTEMPTY;
  }

  if (dir_inode != 0) {
    free_block(dir_inode->block); //free the block and inode associated with the dir
    free_inode(inum);
  }

  //after we free inode and block we now officially remove the directory from the parent

  rv = directory_delete(parent, name);
  free(path_copy);

  printf("rmdir(%s) -> %d\n", path, rv);
  return rv;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char* from, const char* to) {
  char* from_copy;
  char* from_name;
  int from_parent_inum = parse_path_parent(from, &from_copy, &from_name);

  if (from_parent_inum < 0) {
    printf("rename(%s => %s) -> %d\n", from, to, from_parent_inum);
    return from_parent_inum;
  }

  inode_t* from_parent = get_inode(from_parent_inum);
  int inum = directory_lookup(from_parent, from_name);
  if (inum < 0) {
    free(from_copy);
    printf("rename(%s => %s) -> %d\n", from, to, -ENOENT);
    return -ENOENT;
  }
  char* to_copy;
  char* to_name;
  int to_parent_inum = parse_path_parent(to, &to_copy, &to_name);

  if (to_parent_inum < 0) {
    free(from_copy);
    printf("rename(%s => %s) -> %d\n", from, to, to_parent_inum);
    return to_parent_inum;
  }

  inode_t* to_parent = get_inode(to_parent_inum);

  if (directory_lookup(to_parent, to_name) >= 0) {
    free(from_copy);
    free(to_copy);
    printf("rename(%s => %s) -> %d\n", from, to, -EEXIST);
    return -EEXIST;
  }

  int rv = directory_delete(from_parent, from_name);
  if (rv < 0) {
    free(from_copy);
    free(to_copy);
    printf("rename(%s => %s) -> %d\n", from, to, rv);
    return rv;
  }

  rv = directory_put(to_parent, to_name, inum);
  if (rv < 0) {
    directory_put(from_parent, from_name, inum);
    free(from_copy);
    free(to_copy);
    printf("rename(%s => %s) -> %d\n", from, to, rv);
    return rv;
  }

  free(from_copy);
  free(to_copy);
  printf("rename(%s => %s) -> %d\n", from, to, 0);
  return 0;
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
