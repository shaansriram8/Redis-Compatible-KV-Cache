#pragma once

#include "net/buffer.h"

namespace kv {

struct Conn {
  explicit Conn(int fd) : fd(fd) {}

  int fd;
  Buffer in;
  Buffer out;
  // Mirrors whether EPOLLOUT is in this fd's epoll mask, so we only call
  // epoll_ctl when it actually needs to change.
  bool want_write = false;
};

} // namespace kv
