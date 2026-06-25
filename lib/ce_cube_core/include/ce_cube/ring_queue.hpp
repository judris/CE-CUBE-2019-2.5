#pragma once

#include <stddef.h>

namespace ce_cube {

template <typename T, size_t kCapacity>
class RingQueue {
 public:
  RingQueue() : head_(0U), tail_(0U), size_(0U) {}

  bool Push(const T& value) {
    if (size_ >= kCapacity) {
      return false;
    }
    storage_[tail_] = value;
    tail_ = (tail_ + 1U) % kCapacity;
    ++size_;
    return true;
  }

  bool Pop(T* value) {
    if ((value == nullptr) || (size_ == 0U)) {
      return false;
    }
    *value = storage_[head_];
    head_ = (head_ + 1U) % kCapacity;
    --size_;
    return true;
  }

  bool Empty() const { return size_ == 0U; }
  bool Full() const { return size_ >= kCapacity; }
  size_t Size() const { return size_; }

 private:
  T storage_[kCapacity];
  size_t head_;
  size_t tail_;
  size_t size_;
};

}  // namespace ce_cube
