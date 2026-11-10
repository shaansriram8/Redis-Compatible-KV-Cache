FROM ubuntu:24.04

# libclang-rt-18-dev carries the sanitizer and libFuzzer runtimes.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      binutils ca-certificates clang libclang-rt-18-dev libstdc++-13-dev cmake make \
      python3 netcat-openbsd procps \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang CXX=clang++
WORKDIR /src
