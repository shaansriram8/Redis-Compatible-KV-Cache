#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "resp/parser.h"

namespace {

// assert() would vanish under NDEBUG; the fuzzer must always check.
void check(bool ok) {
  if (!ok)
    std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view in(reinterpret_cast<const char*>(data), size);
  std::vector<std::string_view> args;

  // Parse as a pipelined stream, the way the server drains its input buffer.
  while (!in.empty()) {
    size_t n = kv::parse_command(in, args);
    if (n == 0 || n == kv::kParseError)
      break;
    check(n <= in.size());
    for (auto a : args)
      check(a.data() >= in.data() && a.data() + a.size() <= in.data() + n);

    // A complete frame minus its last byte must be partial, never complete
    // and never an error. Catches accepting truncated frames.
    std::vector<std::string_view> scratch;
    check(kv::parse_command(in.substr(0, n - 1), scratch) == 0);

    in.remove_prefix(n);
  }
  return 0;
}
