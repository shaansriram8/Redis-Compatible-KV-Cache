#include "resp/parser.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "net/buffer.h"

using kv::kParseError;
using kv::parse_command;
using Args = std::vector<std::string_view>;

namespace {

// want owns its strings. Views into a "..."s temporary dangle, which is how
// the first version of this test compared args against garbage.
struct Frame {
  std::string bytes;
  std::vector<std::string> want;
};

std::vector<std::string> owned(const Args& args) { return {args.begin(), args.end()}; }

// std::string literals so embedded \0 and \r\n survive.
using namespace std::string_literals;

const std::vector<Frame> kFrames = {
    {"*1\r\n$4\r\nPING\r\n", {"PING"}},
    {"*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n", {"SET", "key", "value"}},
    {"*2\r\n$3\r\nGET\r\n$0\r\n\r\n", {"GET", ""}},
    {"*2\r\n$4\r\nECHO\r\n$6\r\na\r\nb\0c\r\n"s, {"ECHO", "a\r\nb\0c"s}},
    {"*0\r\n", {}},
    {"*-1\r\n", {}},
};

} // namespace

TEST(Parser, ParsesWholeFrames) {
  Args args;
  for (const auto& f : kFrames) {
    EXPECT_EQ(parse_command(f.bytes, args), f.bytes.size()) << f.bytes;
    EXPECT_EQ(owned(args), f.want) << f.bytes;
  }
}

TEST(Parser, EveryPrefixIsPartial) {
  Args args;
  for (const auto& f : kFrames) {
    for (size_t k = 0; k < f.bytes.size(); k++)
      ASSERT_EQ(parse_command(std::string_view(f.bytes).substr(0, k), args), 0u)
          << "frame " << f.bytes << " cut at " << k;
  }
}

// The way the server sees it: the first read delivers part of the frame, the
// second delivers the rest, and the buffer may grow and move in between.
TEST(Parser, SplitAcrossTwoReadsAtEveryOffset) {
  Args args;
  for (const auto& f : kFrames) {
    for (size_t k = 0; k <= f.bytes.size(); k++) {
      kv::Buffer buf(8);
      buf.append(std::string_view(f.bytes).substr(0, k));
      size_t n = parse_command(buf.readable(), args);
      if (k < f.bytes.size())
        ASSERT_EQ(n, 0u) << "cut at " << k;
      buf.append(std::string_view(f.bytes).substr(k));
      ASSERT_EQ(parse_command(buf.readable(), args), f.bytes.size()) << "cut at " << k;
      ASSERT_EQ(owned(args), f.want) << "cut at " << k;
    }
  }
}

TEST(Parser, PipelinedFramesOneAtATime) {
  std::string in;
  for (const auto& f : kFrames)
    in += f.bytes;

  Args args;
  std::string_view rest = in;
  for (const auto& f : kFrames) {
    size_t n = parse_command(rest, args);
    ASSERT_EQ(n, f.bytes.size());
    EXPECT_EQ(owned(args), f.want);
    rest.remove_prefix(n);
  }
  EXPECT_TRUE(rest.empty());
}

TEST(Parser, RejectsMalformed) {
  const char* bad[] = {
      "PING\r\n",                      // inline commands aren't supported
      "$4\r\nPING\r\n",                // bulk without an array
      "*x\r\n",                        // non-numeric count
      "*\r\n",                         // empty count
      "*+1\r\n",                       // from_chars rejects '+', so do we
      "*1\n$4\r\nPING\r\n",            // LF without CR
      "*1\rX",                         // CR not followed by LF
      "*1\r\n:4\r\nPING\r\n",          // wrong type marker for an arg
      "*1\r\n$-1\r\n",                 // null bulk isn't valid in a request
      "*1\r\n$4\r\nPINGXY",            // payload longer than declared
      "*1\r\n$600000000\r\n",          // over proto-max-bulk-len
      "*2000000\r\n",                  // over the arg cap
      "*123456789012345678901234\r\n", // header too long
  };
  Args args;
  for (const char* in : bad)
    EXPECT_EQ(parse_command(in, args), kParseError) << in;
}

// A header with no CR yet is only partial while it could still be valid.
TEST(Parser, LongHeaderWithoutCrIsAnError) {
  Args args;
  EXPECT_EQ(parse_command("*12345", args), 0u);
  EXPECT_EQ(parse_command(std::string(64, '9').insert(0, "*"), args), kParseError);
}
