#include "net/buffer.h"

#include <cstring>

namespace kv {

Buffer::Buffer(size_t cap) : buf_(std::make_unique_for_overwrite<char[]>(cap)), cap_(cap) {
  assert(cap > 0);
}

void Buffer::append(std::string_view s) {
  reserve(s.size());
  std::memcpy(write_ptr(), s.data(), s.size());
  commit(s.size());
}

void Buffer::reserve(size_t n) {
  if (writable() >= n)
    return;
  // Sliding the live bytes down is cheaper than growing when the dead prefix
  // alone makes room, and it keeps a long-lived connection's buffer from
  // creeping upward forever.
  if (cap_ - size() >= n) {
    compact();
    return;
  }
  size_t cap = cap_;
  while (cap - size() < n)
    cap *= 2;
  auto buf = std::make_unique_for_overwrite<char[]>(cap);
  std::memcpy(buf.get(), buf_.get() + r_, size());
  w_ = size();
  r_ = 0;
  buf_ = std::move(buf);
  cap_ = cap;
}

void Buffer::consume(size_t n) {
  assert(n <= size());
  r_ += n;
  if (r_ == w_) {
    // Common case once a reply is fully written: free reset, no memmove.
    r_ = w_ = 0;
  } else if (r_ > cap_ / 2) {
    compact();
  }
}

void Buffer::compact() {
  if (r_ == 0)
    return;
  std::memmove(buf_.get(), buf_.get() + r_, size());
  w_ -= r_;
  r_ = 0;
}

} // namespace kv
