#include "store/flat_map.h"

#include <bit>
#include <cassert>
#include <functional>

namespace kv {

namespace {

size_t hash(std::string_view key) { return std::hash<std::string_view>{}(key); }

} // namespace

FlatMap::FlatMap(size_t cap) : slots_(std::bit_ceil(cap < 8 ? 8 : cap)), mask_(slots_.size() - 1) {}

size_t FlatMap::probe(std::string_view key) const {
  for (size_t i = hash(key) & mask_;; i = (i + 1) & mask_) {
    const Slot& s = slots_[i];
    if (s.state == State::Empty)
      return i;
    if (s.state == State::Full && s.key == key)
      return i;
  }
}

const std::string* FlatMap::get(std::string_view key) const {
  const Slot& s = slots_[probe(key)];
  return s.state == State::Full ? &s.val : nullptr;
}

bool FlatMap::set(std::string_view key, std::string_view val) {
  // Tombstones count toward the load factor: probes walk past them just like
  // live entries, and without this a delete-heavy workload could fill every
  // slot and make a miss loop forever.
  // If it's mostly tombstones, rehashing in place is enough.
  if ((size_ + tombs_ + 1) * 4 > capacity() * 3)
    rehash(size_ >= capacity() / 4 ? capacity() * 2 : capacity());

  size_t tomb = SIZE_MAX;
  size_t i = hash(key) & mask_;
  for (;; i = (i + 1) & mask_) {
    Slot& s = slots_[i];
    if (s.state == State::Empty)
      break;
    if (s.state == State::Tomb) {
      if (tomb == SIZE_MAX)
        tomb = i;
    } else if (s.key == key) {
      s.val = val;
      return false;
    }
  }

  if (tomb != SIZE_MAX) {
    i = tomb;
    tombs_--;
  }
  Slot& s = slots_[i];
  s.state = State::Full;
  s.key = key;
  s.val = val;
  size_++;
  return true;
}

bool FlatMap::erase(std::string_view key) {
  Slot& s = slots_[probe(key)];
  if (s.state != State::Full)
    return false;
  s.state = State::Tomb;
  // Free the strings now rather than at the next rehash, which may never come.
  s.key = std::string();
  s.val = std::string();
  size_--;
  tombs_++;
  return true;
}

void FlatMap::rehash(size_t cap) {
  assert(std::has_single_bit(cap) && cap > size_);
  std::vector<Slot> old(cap);
  old.swap(slots_);
  mask_ = cap - 1;
  tombs_ = 0;

  for (Slot& o : old) {
    if (o.state != State::Full)
      continue;
    size_t i = hash(o.key) & mask_;
    while (slots_[i].state != State::Empty)
      i = (i + 1) & mask_;
    slots_[i] = std::move(o);
  }
}

} // namespace kv
