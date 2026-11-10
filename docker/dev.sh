#!/bin/sh
# Runs a command (default: a shell) in the Linux dev container.
# build/ is a named volume so the Linux build tree never collides with the
# macOS one in the same checkout.
set -e
cd "$(dirname "$0")/.."
docker build -q -t kvcache-dev -f docker/dev.Dockerfile docker >/dev/null

[ $# -eq 0 ] && set -- bash
tty=
[ -t 0 ] && tty=-t
exec docker run --rm -i $tty -v "$PWD":/src -v kvcache-build:/src/build kvcache-dev "$@"
