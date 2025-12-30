#pragma once

#include <sys/stat.h>

extern "C" {
#include "storage.h"
}

namespace nufs {

class Storage {
public:
  static void init(const char* image_path) {
    storage_init(image_path);
  }

  static int inodeByPath(const char* path) {
    return get_inode_by_path(path);
  }

  static int parsePathParent(const char* path, char** path_copy_out, char** name_out) {
    return parse_path_parent(path, path_copy_out, name_out);
  }

  static int statPath(const char* path, struct stat* st) {
    return storage_stat(path, st);
  }

  static char** list(const char* path, int* count) {
    return storage_list(path, count);
  }

  static void listFree(char** list, int count) {
    storage_list_free(list, count);
  }

  static void destroy() {
    storage_destroy();
  }
};

} // namespace nufs


