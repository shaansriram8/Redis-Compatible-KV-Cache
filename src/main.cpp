#include "net/buffer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace {

[[noreturn]] void die(const char* what) {
  std::perror(what);
  std::exit(1);
}

int listen_on(uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
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

// Returns false once the peer is gone.
bool write_all(int fd, kv::Buffer& out) {
  while (!out.empty()) {
    auto bytes = out.readable();
    ssize_t n = write(fd, bytes.data(), bytes.size());
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    out.consume(n);
  }
  return true;
}

void echo(int fd) {
  kv::Buffer buf;
  for (;;) {
    buf.reserve(4096);
    ssize_t n = read(fd, buf.write_ptr(), buf.writable());
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return;
    buf.commit(n);
    if (!write_all(fd, buf))
      return;
  }
}

} // namespace

int main(int argc, char** argv) {
  uint16_t port = argc > 1 ? std::atoi(argv[1]) : 6380;

  // A client that disconnects mid-reply would otherwise kill the whole
  // process on the next write. With this, write just fails with EPIPE.
  std::signal(SIGPIPE, SIG_IGN);

  int lfd = listen_on(port);
  std::printf("listening on %u\n", port);

  for (;;) {
    int fd = accept(lfd, nullptr, nullptr);
    if (fd < 0) {
      if (errno == EINTR)
        continue;
      die("accept");
    }
    echo(fd);
    close(fd);
  }
}
