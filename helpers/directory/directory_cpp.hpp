#pragma once

extern "C" {
#include "directory.h"
}

namespace nufs {

class Directory {
public:
  static int lookup(inode_t* di, const char* name) {
    return directory_lookup(di, name);
  }

  static int put(inode_t* di, const char* name, int inum) {
    return directory_put(di, name, inum);
  }

  static int remove(inode_t* di, const char* name) {
    return directory_delete(di, name);
  }

  static void print(inode_t* dd) {
    print_directory(dd);
  }
};

} // namespace nufs


