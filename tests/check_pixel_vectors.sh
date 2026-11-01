#!/bin/sh
# Host test harness only: sanitizers/libc not linked in freestanding kernel.
set -eu
command -v clang >/dev/null 2>&1 || { echo 'TOKEN_VAZIO_NO_CLANG'; exit 2; }
cd "$(dirname "$0")/.."
d="$(mktemp -d)"
trap 'rm -rf "$d"' EXIT INT TERM
clang -std=c11 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Iinternal tests/pixel_vectors.c -o "$d/pixel_vectors"
"$d/pixel_vectors"
echo 'PASS_SCOPED: Pixel Manifold V1 host C scalar kernel with ASan/UBSan'
