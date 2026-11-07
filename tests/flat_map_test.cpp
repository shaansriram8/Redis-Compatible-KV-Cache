#include "store/flat_map.h"

#include <gtest/gtest.h>

#include <random>
#include <string>
#include <unordered_map>

using kv::FlatMap;

TEST(FlatMap, SetGetErase) {
  FlatMap m;
  EXPECT_EQ(m.get("a"), nullptr);
  EXPECT_TRUE(m.set("a", "1"));
  EXPECT_FALSE(m.set("a", "2"));
  ASSERT_NE(m.get("a"), nullptr);
  EXPECT_EQ(*m.get("a"), "2");
  EXPECT_TRUE(m.erase("a"));
  EXPECT_FALSE(m.erase("a"));
  EXPECT_EQ(m.get("a"), nullptr);
  EXPECT_EQ(m.size(), 0u);
}

TEST(FlatMap, EmptyKeyAndValue) {
  FlatMap m;
  m.set("", "");
  ASSERT_NE(m.get(""), nullptr);
  EXPECT_EQ(*m.get(""), "");
}

TEST(FlatMap, Grows) {
  FlatMap m;
  for (int i = 0; i < 10000; i++)
    m.set(std::to_string(i), std::to_string(i * 2));
  EXPECT_EQ(m.size(), 10000u);
  for (int i = 0; i < 10000; i++)
    EXPECT_EQ(*m.get(std::to_string(i)), std::to_string(i * 2));
}

// If tombstones didn't count toward the load factor, this would fill every
// slot and the final miss would never find an Empty slot to stop at.
TEST(FlatMap, TombstoneChurnDoesNotGrow) {
  FlatMap m(16);
  for (int i = 0; i < 100000; i++) {
    m.set("k" + std::to_string(i), "v");
    m.erase("k" + std::to_string(i));
  }
  EXPECT_EQ(m.size(), 0u);
  EXPECT_EQ(m.capacity(), 16u);
  EXPECT_EQ(m.get("missing"), nullptr);
}

TEST(FlatMap, MatchesUnorderedMap) {
  FlatMap m;
  std::unordered_map<std::string, std::string> ref;
  std::mt19937 rng(42);
  // A small keyspace so sets overwrite, erases hit, and tombstones get reused.
  std::uniform_int_distribution<int> key_dist(0, 5000);
  std::uniform_int_distribution<int> op_dist(0, 9);

  for (int n = 0; n < 1'000'000; n++) {
    std::string key = "key:" + std::to_string(key_dist(rng));
    int op = op_dist(rng);
    if (op < 4) {
      std::string val = std::to_string(rng());
      bool added = ref.insert_or_assign(key, val).second;
      ASSERT_EQ(m.set(key, val), added) << "op " << n;
    } else if (op < 7) {
      ASSERT_EQ(m.erase(key), ref.erase(key) == 1) << "op " << n;
    } else {
      auto it = ref.find(key);
      const std::string* got = m.get(key);
      ASSERT_EQ(got != nullptr, it != ref.end()) << "op " << n;
      if (got)
        ASSERT_EQ(*got, it->second) << "op " << n;
    }
    ASSERT_EQ(m.size(), ref.size());
  }

  for (int k = 0; k <= 5000; k++) {
    std::string key = "key:" + std::to_string(k);
    auto it = ref.find(key);
    const std::string* got = m.get(key);
    ASSERT_EQ(got != nullptr, it != ref.end()) << key;
    if (got)
      EXPECT_EQ(*got, it->second);
  }
}
