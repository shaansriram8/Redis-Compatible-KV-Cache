#include <gtest/gtest.h>

#include <memory>

TEST(Sanity, Builds) { EXPECT_EQ(1 + 1, 2); }

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define KV_HAS_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define KV_HAS_ASAN 1
#endif

#ifdef KV_HAS_ASAN
// Guards against the debug preset silently losing -fsanitize=address. Without
// it, every later "clean under ASan" check would pass for the wrong reason.
TEST(SanityDeathTest, AsanCatchesHeapOverflow) {
  EXPECT_DEATH(
      {
        auto p = std::make_unique<char[]>(8);
        volatile int i = 8;
        p[i] = 'x';
      },
      "heap-buffer-overflow");
}
#endif
