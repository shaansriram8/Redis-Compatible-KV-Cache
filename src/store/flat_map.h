#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

// Open addressing with linear probing. Deletes leave tombstones so probe
// chains that run through the deleted slot stay intact; they are cleared on
// the next rehash.
class FlatMap {
public:
  explicit FlatMap(size_t cap = 16);

  // Returns nullptr on a miss. The pointer is invalidated by any set() or erase().
  const std::string* get(std::string_view key) const;

  // Returns true if the key was new.
  bool set(std::string_view key, std::string_view val);
  bool erase(std::string_view key);

  size_t size() const { return size_; }
  size_t capacity() const { return slots_.size(); }

private:
  enum class State : uint8_t { Empty, Full, Tomb };

  struct Slot {
    State state = State::Empty;
    std::string key;
    std::string val;
  };

  // Index of the slot holding key, or of the Empty slot that ended the probe.
  size_t probe(std::string_view key) const;
  void rehash(size_t cap);

  std::vector<Slot> slots_;
  size_t mask_;
  size_t size_ = 0;
  size_t tombs_ = 0;
};

} // namespace kv
