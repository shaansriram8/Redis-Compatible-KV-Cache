#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <string_view>

namespace kv {

// Byte queue for socket I/O. Bytes are appended at w_ and consumed from r_:
//
//   [ consumed | readable | writable ]
//   0          r_         w_         cap_
//
// Views from readable() point into the buffer and are invalidated by any
// call that can move data (append, reserve, consume).
class Buffer {
public:
  static constexpr size_t kDefaultCap = 4096;

  explicit Buffer(size_t cap = kDefaultCap);

  Buffer(Buffer&&) noexcept = default;
  Buffer& operator=(Buffer&&) noexcept = default;
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  void append(std::string_view s);

  // For read(2) straight into the buffer: reserve(n), read into write_ptr(),
  // then commit() however many bytes actually arrived.
  void reserve(size_t n);
  char* write_ptr() { return buf_.get() + w_; }
  size_t writable() const { return cap_ - w_; }
  void commit(size_t n) {
    assert(n <= writable());
    w_ += n;
  }

  std::string_view readable() const { return {buf_.get() + r_, w_ - r_}; }
  void consume(size_t n);

  size_t size() const { return w_ - r_; }
  bool empty() const { return r_ == w_; }
  size_t capacity() const { return cap_; }

private:
  void compact();

  std::unique_ptr<char[]> buf_;
  size_t cap_;
  size_t r_ = 0;
  size_t w_ = 0;
};

} // namespace kv
