#!/bin/sh
# Host-only offline negative fixtures; host sanitizers are test dependencies,
# not libraries linked into the freestanding BrowserRaf production executable.
set -eu
command -v clang >/dev/null 2>&1 || { echo "MISSING_TOOL clang"; exit 2; }
cd "$(dirname "$0")/.."
d="$(mktemp -d)"
trap 'rm -rf "$d"' EXIT HUP INT TERM
clang -std=c11 -O1 -g -Wall -Wextra -Wno-unused-function \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I internal tests/negative_fixtures.c -o "$d/negative_fixtures"
clang -std=c11 -O1 -g -Wall -Wextra -Wno-unused-function \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I internal tests/test_division.c -o "$d/test_division"
"$d/test_division"
"$d/negative_fixtures"
echo 'PASS_SCOPED: host offline URL/HTTP/HTML negative fixtures with ASan/UBSan'
