#include "resp/parser.h"

#include <charconv>

namespace kv {

namespace {

// kMaxBulkLen is Redis's default proto-max-bulk-len. Both caps bound how much
// a client can make us buffer for a single command.
constexpr int64_t kMaxArgs = 1024 * 1024;
constexpr int64_t kMaxBulkLen = 512 * 1024 * 1024;

// '*' or '$', at most 20 chars of integer, "\r\n". A longer header is garbage,
// and rejecting it beats buffering forever waiting for a CR.
constexpr size_t kMaxHeaderLen = 1 + 20 + 2;

enum class Status { Ok, Partial, Error };

// Reads "<prefix><integer>\r\n" at pos and moves pos past it.
Status read_header(std::string_view in, size_t& pos, char prefix, int64_t& n) {
  if (pos == in.size())
    return Status::Partial;
  if (in[pos] != prefix)
    return Status::Error;

  std::string_view line = in.substr(pos, kMaxHeaderLen);
  size_t cr = line.find('\r');
  if (cr == std::string_view::npos)
    return line.size() < kMaxHeaderLen ? Status::Partial : Status::Error;
  size_t lf = pos + cr + 1;
  if (lf == in.size())
    return Status::Partial;
  if (in[lf] != '\n')
    return Status::Error;

  const char* first = in.data() + pos + 1;
  const char* last = in.data() + pos + cr;
  auto [end, ec] = std::from_chars(first, last, n);
  if (ec != std::errc() || end != last)
    return Status::Error;
  pos = lf + 1;
  return Status::Ok;
}

} // namespace

// A partial frame is reparsed from the start on the next read. That's simpler
// than keeping resumable state, and cheap: only headers get scanned, never the
// bulk payloads, which are skipped by length.
size_t parse_command(std::string_view in, std::vector<std::string_view>& args) {
  args.clear();
  size_t pos = 0;

  int64_t argc;
  Status s = read_header(in, pos, '*', argc);
  if (s != Status::Ok)
    return s == Status::Partial ? 0 : kParseError;
  if (argc > kMaxArgs)
    return kParseError;
  // Redis quietly skips "*0" and "*-1" in requests.
  if (argc <= 0)
    return pos;

  for (int64_t i = 0; i < argc; i++) {
    int64_t len;
    s = read_header(in, pos, '$', len);
    if (s != Status::Ok)
      return s == Status::Partial ? 0 : kParseError;
    if (len < 0 || len > kMaxBulkLen)
      return kParseError;

    size_t n = static_cast<size_t>(len);
    if (in.size() - pos < n + 2)
      return 0;
    if (in[pos + n] != '\r' || in[pos + n + 1] != '\n')
      return kParseError;
    args.push_back(in.substr(pos, n));
    pos += n + 2;
  }
  return pos;
}

} // namespace kv
