#include <cstdio>
#include <cstdlib>

#include "net/server.h"

int main(int argc, char** argv) {
  uint16_t port = argc > 1 ? std::atoi(argv[1]) : 6380;
  kv::Server server(port);
  std::fprintf(stderr, "listening on %u\n", port);
  server.run();
}
