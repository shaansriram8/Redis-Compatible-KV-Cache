#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace kv {

inline constexpr size_t kParseError = SIZE_MAX;

// Parses one command, a RESP array of bulk strings, from the front of in.
//
// Returns the bytes consumed, or 0 on a partial frame (keep the bytes and
// retry after the next read), or kParseError on malformed input. There's no
// resyncing a broken stream, so the caller should close the connection.
//
// args gets views into in; they die when the buffer is consumed or grown.
// An empty array ("*0\r\n") parses to no args and should just be skipped.
size_t parse_command(std::string_view in, std::vector<std::string_view>& args);

} // namespace kv
