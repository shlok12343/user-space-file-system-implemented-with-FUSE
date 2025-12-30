// C++ NUFS driver, parallel to the existing C nufs.c.

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>
#include <unistd.h>

#define FUSE_USE_VERSION 26

extern "C" {
#include <fuse.h>

#include "helpers/blocks/blocks.h"
#include "helpers/directory/directory.h"
#include "helpers/storage/inode.h"
#include "helpers/storage/storage.h"
}

#include "helpers/storage/storage_cpp.hpp"

// initialize a new inode with standard fields (C++ version)
static void init_inode_cpp(inode_t* inode, mode_t mode) {
  inode->mode = mode;
  inode->uid = getuid();
  inode->gid = getgid();
  inode->size = 0;
  inode->refs = 1;
  inode->mtime = time(nullptr);
  inode->block = 0;
}

// Core FUSE operations (access, getattr, readdir, mknod, mkdir, unlink, link, rmdir, rename)

int nufs_access_cpp(const char* path, int mask) {
  int rv = 0;

  int inum = get_inode_by_path(path);
  if (inum < 0) {
    rv = -ENOENT;
  }

  std::printf("access(%s, %04o) -> %d\n", path, mask, rv);
  return rv;
}

int nufs_getattr_cpp(const char* path, struct stat* st) {
  int rv = nufs::Storage::statPath(path, st);
  std::printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n",
              path, rv, st->st_mode, st->st_size);
  return rv;
}

int nufs_readdir_cpp(const char* path, void* buf, fuse_fill_dir_t filler,
                     off_t offset, struct fuse_file_info* fi) {
  struct stat st;
  int rv = 0;

  rv = nufs_getattr_cpp(path, &st);
  if (rv < 0) {
    std::printf("readdir(%s) -> %d\n", path, rv);
    return rv;
  }

  filler(buf, ".", &st, 0);

  if (std::strcmp(path, "/") != 0) {
    char parent_path[256];
    std::strncpy(parent_path, path, sizeof(parent_path));
    parent_path[sizeof(parent_path) - 1] = '\0';

    char* last_slash = std::strrchr(parent_path, '/');
    if (last_slash != nullptr) {
      if (last_slash == parent_path) {
        std::strcpy(parent_path, "/");
      } else {
        *last_slash = '\0';
      }
      rv = nufs_getattr_cpp(parent_path, &st);
      if (rv == 0) {
        filler(buf, "..", &st, 0);
      }
    }
  }

  int count = 0;
  char** list = nufs::Storage::list(path, &count);

  for (int i = 0; i < count; i++) {
    char child_path[256];
    if (std::strcmp(path, "/") == 0) {
      std::snprintf(child_path, sizeof(child_path), "/%s", list[i]);
    } else {
      std::snprintf(child_path, sizeof(child_path), "%s/%s", path, list[i]);
    }

    rv = nufs_getattr_cpp(child_path, &st);
    if (rv == 0) {
      filler(buf, list[i], &st, 0);
    }
  }

  nufs::Storage::listFree(list, count);

  std::printf("readdir(%s) -> %d\n", path, 0);
  return 0;
}

int nufs_mknod_cpp(const char* path, mode_t mode, dev_t rdev) {
  char* path_copy;
  char* name;

  int parent_inum = parse_path_parent(path, &path_copy, &name);
  if (parent_inum < 0) {
    std::printf("mknod(%s, %04o) -> %d\n", path, mode, parent_inum);
    return parent_inum;
  }

  inode_t* parent = get_inode(parent_inum);

  if (directory_lookup(parent, name) >= 0) {
    std::free(path_copy);
    std::printf("mknod(%s, %04o) -> %d\n", path, mode, -EEXIST);
    return -EEXIST;
  }

  int new_inum = alloc_inode();
  if (new_inum < 0) {
    std::free(path_copy);
    std::printf("mknod(%s, %04o) -> %d\n", path, mode, -ENOSPC);
    return -ENOSPC;
  }

  inode_t* new_inode = get_inode(new_inum);
  init_inode_cpp(new_inode, mode);

  int rv = directory_put(parent, name, new_inum);
  if (rv < 0) {
    free_inode(new_inum);
    std::free(path_copy);
    std::printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
  }

  std::free(path_copy);
  std::printf("mknod(%s, %04o) -> %d\n", path, mode, 0);
  return 0;
}

int nufs_mkdir_cpp(const char* path, mode_t mode) {
  char* path_copy;
  char* name;
  int parent_inum = parse_path_parent(path, &path_copy, &name);
  if (parent_inum < 0) {
    std::printf("mkdir(%s) -> %d\n", path, parent_inum);
    return parent_inum;
  }

  inode_t* parent = get_inode(parent_inum);
  (void)parent;

  int new_inum = alloc_inode();
  if (new_inum < 0) {
    std::free(path_copy);
    std::printf("mkdir(%s) -> %d\n", path, -ENOSPC);
    return -ENOSPC;
  }
  inode_t* new_dir_inode = get_inode(new_inum);
  init_inode_cpp(new_dir_inode, S_IFDIR | mode);
  new_dir_inode->block = alloc_block();
  if (new_dir_inode->block < 0) {
    free_inode(new_inum);
    std::free(path_copy);
    std::printf("mkdir(%s) -> %d\n", path, -ENOSPC);
    return -ENOSPC;
  }

  int rv = directory_put(parent, name, new_inum);
  if (rv < 0) {
    free_block(new_dir_inode->block);
    free_inode(new_inum);
  }

  std::free(path_copy);
  std::printf("mkdir(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_unlink_cpp(const char* path) {
  char* path_copy;
  char* name;

  int parent_inum = parse_path_parent(path, &path_copy, &name);
  if (parent_inum < 0) {
    std::printf("unlink(%s) -> %d\n", path, parent_inum);
    return parent_inum;
  }

  inode_t* parent = get_inode(parent_inum);

  int inum = directory_lookup(parent, name);
  if (inum < 0) {
    std::free(path_copy);
    std::printf("unlink(%s) -> %d\n", path, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  if (S_ISDIR(node->mode)) {
    std::free(path_copy);
    std::printf("unlink(%s) -> %d\n", path, -EISDIR);
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
  std::free(path_copy);

  std::printf("unlink(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_link_cpp(const char* from, const char* to) {
  int rv = -1;
  std::printf("link(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_rmdir_cpp(const char* path) {
  int rv = -1;
  std::printf("rmdir(%s) -> %d\n", path, rv);

  char* path_copy;
  char* name;

  int parent_inum = parse_path_parent(path, &path_copy, &name);
  if (parent_inum < 0) {
    std::free(path_copy);
    std::printf("rmdir(%s) -> %d\n", path, parent_inum);
    return parent_inum;
  }

  inode_t* parent = get_inode(parent_inum);

  int inum = directory_lookup(parent, name);
  if (inum < 0) {
    std::free(path_copy);
    std::printf("rmdir(%s) -> %d\n", path, -ENOENT);
    return -ENOENT;
  }

  inode_t* dir_inode = get_inode(inum);

  if (dir_inode->size > 0) {
    std::free(path_copy);
    std::printf("rmdir(%s) -> %d\n", path, -ENOTEMPTY);
    return -ENOTEMPTY;
  }

  if (dir_inode != nullptr) {
    free_block(dir_inode->block);
    free_inode(inum);
  }

  rv = directory_delete(parent, name);
  std::free(path_copy);

  std::printf("rmdir(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_rename_cpp(const char* from, const char* to) {
  char* from_copy;
  char* from_name;
  int from_parent_inum = parse_path_parent(from, &from_copy, &from_name);

  if (from_parent_inum < 0) {
    std::printf("rename(%s => %s) -> %d\n", from, to, from_parent_inum);
    return from_parent_inum;
  }

  inode_t* from_parent = get_inode(from_parent_inum);
  int inum = directory_lookup(from_parent, from_name);
  if (inum < 0) {
    std::free(from_copy);
    std::printf("rename(%s => %s) -> %d\n", from, to, -ENOENT);
    return -ENOENT;
  }

  char* to_copy;
  char* to_name;
  int to_parent_inum = parse_path_parent(to, &to_copy, &to_name);

  if (to_parent_inum < 0) {
    std::free(from_copy);
    std::printf("rename(%s => %s) -> %d\n", from, to, to_parent_inum);
    return to_parent_inum;
  }

  inode_t* to_parent = get_inode(to_parent_inum);

  if (directory_lookup(to_parent, to_name) >= 0) {
    std::free(from_copy);
    std::free(to_copy);
    std::printf("rename(%s => %s) -> %d\n", from, to, -EEXIST);
    return -EEXIST;
  }

  int rv = directory_delete(from_parent, from_name);
  if (rv < 0) {
    std::free(from_copy);
    std::free(to_copy);
    std::printf("rename(%s => %s) -> %d\n", from, to, rv);
    return rv;
  }

  rv = directory_put(to_parent, to_name, inum);
  if (rv < 0) {
    directory_put(from_parent, from_name, inum);
    std::free(from_copy);
    std::free(to_copy);
    std::printf("rename(%s => %s) -> %d\n", from, to, rv);
    return rv;
  }

  std::free(from_copy);
  std::free(to_copy);
  std::printf("rename(%s => %s) -> %d\n", from, to, 0);
  return 0;
}

// IO-related and other operations

int nufs_chmod_cpp(const char* path, mode_t mode) {
  int rv = -1;
  std::printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

int nufs_truncate_cpp(const char* path, off_t size) {
  int inum = get_inode_by_path(path);
  if (inum < 0) {
    std::printf("truncate(%s, %ld bytes) -> %d\n", path, size, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  if (S_ISDIR(node->mode)) {
    std::printf("truncate(%s, %ld bytes) -> %d\n", path, size, -EISDIR);
    return -EISDIR;
  }

  if (size > 4096) {
    std::printf("truncate(%s, %ld bytes) -> %d\n", path, size, -EFBIG);
    return -EFBIG;
  }

  if (size > 0 && node->block == 0) {
    node->block = alloc_block();
    if (node->block < 0) {
      std::printf("truncate(%s, %ld bytes) -> %d\n", path, size, -ENOSPC);
      return -ENOSPC;
    }
  }

  if (size == 0 && node->block != 0) {
    free_block(node->block);
    node->block = 0;
  }

  node->size = size;
  node->mtime = time(nullptr);

  std::printf("truncate(%s, %ld bytes) -> %d\n", path, size, 0);
  return 0;
}

int nufs_open_cpp(const char* path, struct fuse_file_info* fi) {
  int rv = 0;
  std::printf("open(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_read_cpp(const char* path, char* buf, size_t size, off_t offset,
                  struct fuse_file_info* fi) {
  int inum = get_inode_by_path(path);
  if (inum < 0) {
    std::printf("read(%s, %ld bytes, @+%ld) -> %d\n",
                path, (long)size, (long)offset, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  if (S_ISDIR(node->mode)) {
    std::printf("read(%s, %ld bytes, @+%ld) -> %d\n",
                path, (long)size, (long)offset, -EISDIR);
    return -EISDIR;
  }

  if (offset >= node->size) {
    std::printf("read(%s, %ld bytes, @+%ld) -> %d\n",
                path, (long)size, (long)offset, 0);
    return 0;
  }

  int available = node->size - offset;
  int bytes_to_read = (size < (size_t)available) ? (int)size : available;

  if (node->block == 0) {
    std::printf("read(%s, %ld bytes, @+%ld) -> %d\n",
                path, (long)size, (long)offset, 0);
    return 0;
  }

  void* block_data = blocks_get_block(node->block);
  std::memcpy(buf, static_cast<char*>(block_data) + offset, bytes_to_read);
  std::printf("read(%s, %ld bytes, @+%ld) -> %d\n",
              path, (long)size, (long)offset, bytes_to_read);
  return bytes_to_read;
}

int nufs_write_cpp(const char* path, const char* buf, size_t size, off_t offset,
                   struct fuse_file_info* fi) {
  int inum = get_inode_by_path(path);
  if (inum < 0) {
    std::printf("write(%s, %ld bytes, @+%ld) -> %d\n",
                path, (long)size, (long)offset, -ENOENT);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);

  if (S_ISDIR(node->mode)) {
    std::printf("write(%s, %ld bytes, @+%ld) -> %d\n",
                path, (long)size, (long)offset, -EISDIR);
    return -EISDIR;
  }

  int required_size = offset + size;

  if (required_size > 4096) {
    std::printf("write(%s, %ld bytes, @+%ld) -> %d\n",
                path, (long)size, (long)offset, -EFBIG);
    return -EFBIG;
  }

  if (node->block == 0) {
    node->block = alloc_block();
    if (node->block < 0) {
      std::printf("write(%s, %ld bytes, @+%ld) -> %d\n",
                  path, (long)size, (long)offset, -ENOSPC);
      return -ENOSPC;
    }
  }

  void* block_data = blocks_get_block(node->block);
  std::memcpy(static_cast<char*>(block_data) + offset, buf, size);
  if (required_size > node->size) {
    node->size = required_size;
  }
  node->mtime = time(nullptr);

  std::printf("write(%s, %ld bytes, @+%ld) -> %d\n",
              path, (long)size, (long)offset, (int)size);
  return (int)size;
}

int nufs_utimens_cpp(const char* path, const struct timespec ts[2]) {
  int rv = -1;
  std::printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n",
              path,
              ts[0].tv_sec, ts[0].tv_nsec,
              ts[1].tv_sec, ts[1].tv_nsec,
              rv);
  return rv;
}

int nufs_ioctl_cpp(const char* path, int cmd, void* arg,
                   struct fuse_file_info* fi,
                   unsigned int flags, void* data) {
  int rv = -1;
  std::printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
  return rv;
}

static void nufs_init_ops_cpp(struct fuse_operations* ops) {
  std::memset(ops, 0, sizeof(struct fuse_operations));
  ops->access   = nufs_access_cpp;
  ops->getattr  = nufs_getattr_cpp;
  ops->readdir  = nufs_readdir_cpp;
  ops->mknod    = nufs_mknod_cpp;
  ops->mkdir    = nufs_mkdir_cpp;
  ops->link     = nufs_link_cpp;
  ops->unlink   = nufs_unlink_cpp;
  ops->rmdir    = nufs_rmdir_cpp;
  ops->rename   = nufs_rename_cpp;
  ops->chmod    = nufs_chmod_cpp;
  ops->truncate = nufs_truncate_cpp;
  ops->open     = nufs_open_cpp;
  ops->read     = nufs_read_cpp;
  ops->write    = nufs_write_cpp;
  ops->utimens  = nufs_utimens_cpp;
  ops->ioctl    = nufs_ioctl_cpp;
}

int main(int argc, char* argv[]) {
  assert(argc > 2 && argc < 6);

  // Last argument is the filesystem image path, as in the C version.
  argc--;
  nufs::Storage::init(argv[argc]);

  struct fuse_operations ops;
  nufs_init_ops_cpp(&ops);

  return fuse_main(argc, argv, &ops, nullptr);
}

