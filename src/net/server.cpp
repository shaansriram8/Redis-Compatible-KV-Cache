#include "net/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace kv {

namespace {

constexpr size_t kReadChunk = 16 * 1024;
constexpr int kMaxEvents = 256;

[[noreturn]] void die(const char* what) {
  std::perror(what);
  std::exit(1);
}

int listen_on(uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0)
    die("socket");

  // Lets the server restart immediately instead of failing with EADDRINUSE
  // while the previous run's sockets sit in TIME_WAIT.
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    die("bind");
  if (listen(fd, SOMAXCONN) < 0)
    die("listen");
  return fd;
}

bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK; }

} // namespace

Server::Server(uint16_t port)
    : listen_fd_(listen_on(port)), epoll_fd_(epoll_create1(EPOLL_CLOEXEC)) {
  if (epoll_fd_ < 0)
    die("epoll_create1");

  // The listener is the only fd registered with a null ptr; every client
  // carries its Conn.
  epoll_event ev{};
  ev.events = EPOLLIN | EPOLLET;
  ev.data.ptr = nullptr;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0)
    die("epoll_ctl");
}

Server::~Server() {
  for (auto& [fd, conn] : conns_)
    close(fd);
  close(epoll_fd_);
  close(listen_fd_);
}

void Server::run() {
  epoll_event events[kMaxEvents];
  for (;;) {
    int n = epoll_wait(epoll_fd_, events, kMaxEvents, -1);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      die("epoll_wait");
    }
    // epoll reports each fd at most once per batch, so dropping a conn here
    // can't leave a dangling ptr in a later event of the same batch.
    for (int i = 0; i < n; i++) {
      auto* conn = static_cast<Conn*>(events[i].data.ptr);
      if (!conn) {
        accept_all();
        continue;
      }
      uint32_t ev = events[i].events;
      // EPOLLHUP goes through the read path too: there may still be unread
      // input, and read() returning 0 is what closes the connection.
      bool ok = !(ev & EPOLLERR);
      if (ok && (ev & (EPOLLIN | EPOLLHUP)))
        ok = on_readable(*conn);
      if (ok && (ev & EPOLLOUT))
        ok = flush(*conn);
      if (!ok)
        drop(*conn);
    }
  }
}

void Server::accept_all() {
  for (;;) {
    int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0) {
      if (errno == EINTR || errno == ECONNABORTED)
        continue;
      if (would_block())
        return;
      // Usually EMFILE. Keep serving existing clients; since the listener is
      // edge-triggered, the rest of the backlog waits for the next connect.
      std::perror("accept4");
      return;
    }

    // Replies are small; without this Nagle holds them back waiting for an
    // ACK that the client's delayed-ACK timer is sitting on (~40 ms).
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    auto conn = std::make_unique<Conn>(fd);
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = conn.get();
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0)
      die("epoll_ctl");
    conns_.emplace(fd, std::move(conn));
  }
}

bool Server::on_readable(Conn& conn) {
  for (;;) {
    conn.in.reserve(kReadChunk);
    ssize_t n = read(conn.fd, conn.in.write_ptr(), conn.in.writable());
    // Keep reading after a short read. If the client's last bytes and its FIN
    // arrived together, their one edge is this event, and only reading on to
    // the 0 return notices the close.
    if (n > 0) {
      conn.in.commit(n);
      continue;
    }
    if (n == 0)
      return false;
    if (errno == EINTR)
      continue;
    if (would_block())
      break;
    return false;
  }

  // Placeholder handler: echo everything back.
  conn.out.append(conn.in.readable());
  conn.in.consume(conn.in.size());
  return flush(conn);
}

bool Server::flush(Conn& conn) {
  while (!conn.out.empty()) {
    auto bytes = conn.out.readable();
    // MSG_NOSIGNAL: a peer that already closed gets us EPIPE instead of a
    // SIGPIPE that kills the process.
    ssize_t n = send(conn.fd, bytes.data(), bytes.size(), MSG_NOSIGNAL);
    if (n >= 0) {
      conn.out.consume(n);
      continue;
    }
    if (errno == EINTR)
      continue;
    if (would_block())
      break;
    return false;
  }

  // Arm EPOLLOUT only while bytes are queued. Left on, it would wake the loop
  // every time the kernel frees send-buffer space, even with nothing to send.
  bool want = !conn.out.empty();
  if (want != conn.want_write) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | (want ? EPOLLOUT : 0);
    ev.data.ptr = &conn;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, conn.fd, &ev) < 0)
      die("epoll_ctl");
    conn.want_write = want;
  }
  return true;
}

void Server::drop(Conn& conn) {
  int fd = conn.fd;
  // Closing the fd also removes it from the epoll set.
  close(fd);
  conns_.erase(fd);
}

} // namespace kv
