#include "net/buffer.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

using kv::Buffer;

TEST(Buffer, StartsEmpty) {
  Buffer b;
  EXPECT_TRUE(b.empty());
  EXPECT_EQ(b.size(), 0u);
  EXPECT_EQ(b.readable(), "");
}

TEST(Buffer, AppendThenRead) {
  Buffer b;
  b.append("hello ");
  b.append("world");
  EXPECT_EQ(b.readable(), "hello world");
}

TEST(Buffer, GrowsPastInitialCapacity) {
  Buffer b(8);
  std::string want;
  for (int i = 0; i < 100; i++) {
    std::string chunk = std::to_string(i) + ",";
    b.append(chunk);
    want += chunk;
  }
  EXPECT_EQ(b.readable(), want);
  EXPECT_GE(b.capacity(), want.size());
}

TEST(Buffer, SingleAppendMuchLargerThanCapacity) {
  Buffer b(4);
  std::string big(10000, 'z');
  b.append(big);
  EXPECT_EQ(b.readable(), big);
}

TEST(Buffer, PartialConsume) {
  Buffer b;
  b.append("abcdef");
  b.consume(2);
  EXPECT_EQ(b.readable(), "cdef");
  b.consume(3);
  EXPECT_EQ(b.readable(), "f");
}

TEST(Buffer, ConsumeEverythingResetsOffsets) {
  Buffer b(16);
  b.append("0123456789");
  b.consume(10);
  EXPECT_TRUE(b.empty());
  // Offsets went back to 0, so the full capacity is writable again.
  EXPECT_EQ(b.writable(), 16u);
}

TEST(Buffer, CompactsOncePastHalfCapacity) {
  Buffer b(16);
  b.append("0123456789ab");
  b.consume(9);
  EXPECT_EQ(b.readable(), "9ab");
  EXPECT_EQ(b.writable(), 13u);
  EXPECT_EQ(b.capacity(), 16u);
}

TEST(Buffer, NoCompactionBelowHalf) {
  Buffer b(16);
  b.append("0123456789ab");
  b.consume(4);
  EXPECT_EQ(b.readable(), "456789ab");
  EXPECT_EQ(b.writable(), 4u);
}

// Room exists only if the consumed prefix is reclaimed; this must not grow.
TEST(Buffer, ReserveCompactsInsteadOfGrowing) {
  Buffer b(16);
  b.append("0123456789ab");
  b.consume(6);
  b.append("cdefghij");
  EXPECT_EQ(b.readable(), "6789abcdefghij");
  EXPECT_EQ(b.capacity(), 16u);
}

TEST(Buffer, GrowthKeepsUnconsumedBytesOnly) {
  Buffer b(8);
  b.append("xxxxabcd");
  b.consume(4);
  b.append("efghijklmnop");
  EXPECT_EQ(b.readable(), "abcdefghijklmnop");
}

TEST(Buffer, ReserveWriteCommit) {
  Buffer b(4);
  b.append("ab");
  b.reserve(10);
  ASSERT_GE(b.writable(), 10u);
  std::memcpy(b.write_ptr(), "cdefghijkl", 10);
  b.commit(10);
  EXPECT_EQ(b.readable(), "abcdefghijkl");
}

// Simulates a socket that hands over odd-sized chunks while the reader drains
// in different-sized chunks, so growth and compaction interleave.
TEST(Buffer, InterleavedAppendConsume) {
  Buffer b(32);
  std::string sent, got;
  int next = 0;
  for (int round = 0; round < 1000; round++) {
    std::string chunk;
    for (int i = 0; i < round % 17 + 1; i++)
      chunk += static_cast<char>('a' + next++ % 26);
    b.append(chunk);
    sent += chunk;
    size_t n = std::min<size_t>(b.size(), round % 13 + 1);
    got += b.readable().substr(0, n);
    b.consume(n);
  }
  got += b.readable();
  b.consume(b.size());
  EXPECT_EQ(got, sent);
  EXPECT_TRUE(b.empty());
}

TEST(Buffer, MoveTransfersContents) {
  Buffer a;
  a.append("payload");
  Buffer b = std::move(a);
  EXPECT_EQ(b.readable(), "payload");
}
