#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "net/conn.h"

namespace kv {

// Single-threaded epoll server. Sockets are edge-triggered, so every handler
// drains its socket until EAGAIN; stopping early loses the edge and the
// connection hangs with data sitting in the kernel.
class Server {
public:
  explicit Server(uint16_t port);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  [[noreturn]] void run();

private:
  void accept_all();
  // These return false when the connection should be dropped.
  bool on_readable(Conn& conn);
  bool flush(Conn& conn);
  void drop(Conn& conn);

  int listen_fd_;
  int epoll_fd_;
  std::unordered_map<int, std::unique_ptr<Conn>> conns_;
};

} // namespace kv
