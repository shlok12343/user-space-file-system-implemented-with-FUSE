#pragma once

#include <string>
#include <vector>

extern "C" {
#include "slist.h"
}

namespace nufs {

class SList {
public:
  explicit SList(slist_t* ptr = nullptr) : ptr_(ptr) {}

  ~SList() {
    if (ptr_) {
      slist_free(ptr_);
    }
  }

  SList(const SList&) = delete;
  SList& operator=(const SList&) = delete;

  SList(SList&& other) noexcept : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  SList& operator=(SList&& other) noexcept {
    if (this != &other) {
      if (ptr_) {
        slist_free(ptr_);
      }
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  static SList explode(const char* text, char delim) {
    return SList(slist_explode(text, delim));
  }

  std::vector<std::string> toVector() const {
    std::vector<std::string> out;
    for (slist_t* it = ptr_; it; it = it->next) {
      if (it->data) {
        out.emplace_back(it->data);
      }
    }
    return out;
  }

  slist_t* get() const { return ptr_; }

private:
  slist_t* ptr_;
};

} // namespace nufs


