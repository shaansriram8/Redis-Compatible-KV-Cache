cmake_minimum_required(VERSION 3.25)
project(kvcache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Comma-separated list passed to -fsanitize=, set by the presets.
set(KV_SANITIZE "" CACHE STRING "Sanitizers to enable")

# Sanitizers go on every target, including GoogleTest, so instrumented and
# uninstrumented code never mix (TSan in particular gives false positives).
if(KV_SANITIZE)
  add_compile_options(-fsanitize=${KV_SANITIZE} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${KV_SANITIZE})
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP ON)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Warnings only on our code; -Werror on third-party code breaks on compiler upgrades.
add_library(kv_warnings INTERFACE)
target_compile_options(kv_warnings INTERFACE -Wall -Wextra -Werror)

add_library(kv STATIC
  src/net/buffer.cpp
  src/store/flat_map.cpp)
target_include_directories(kv PUBLIC src)
target_link_libraries(kv PRIVATE kv_warnings)

add_executable(kvcache src/main.cpp)
target_link_libraries(kvcache PRIVATE kv kv_warnings)

enable_testing()
add_executable(kv_tests
  tests/sanity_test.cpp
  tests/buffer_test.cpp
  tests/flat_map_test.cpp)
target_link_libraries(kv_tests PRIVATE kv kv_warnings GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(kv_tests)

# Only the release preset turns this on; sanitizer numbers are meaningless.
option(KV_BENCH "Build benchmarks" OFF)
if(KV_BENCH)
  FetchContent_Declare(
    benchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)

  add_executable(flat_map_bench bench/flat_map_bench.cpp)
  target_link_libraries(flat_map_bench PRIVATE kv kv_warnings benchmark::benchmark)
endif()
