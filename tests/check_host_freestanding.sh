#!/bin/sh
set -eu
command -v clang >/dev/null 2>&1 || { echo 'MISSING_TOOL clang'; exit 2; }
command -v readelf >/dev/null 2>&1 || { echo 'MISSING_TOOL readelf'; exit 2; }
cd "$(dirname "$0")/.."
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
clang -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie -fno-pic -nostdlib -static -Wl,-e,_start -Wl,--build-id=none -Wl,-z,noexecstack -Iinternal internal/br_start.S internal/br_main.c -o "$tmpdir/browserraf"
readelf -h "$tmpdir/browserraf" | grep -q ELF64
if readelf -l "$tmpdir/browserraf" | grep -q INTERP; then echo "FAIL: INTERP"; exit 1; fi
if readelf -d "$tmpdir/browserraf" 2>/dev/null | grep -q NEEDED; then echo "FAIL: NEEDED"; exit 1; fi
grep -Fq 'HTTPS não suportado neste build' internal/br_main.c || { echo "FAIL: HTTPS safety marker"; exit 1; }
# Exercise the actual host binary with an HTTPS URL. DNS/TCP must never start.
"$tmpdir/browserraf" https://127.0.0.1/ > "$tmpdir/https.out" 2>&1
grep -Fq 'HTTPS não suportado neste build' "$tmpdir/https.out" || { echo 'FAIL HTTPS was not refused'; exit 1; }
if grep -Eq 'Resolvendo DNS|Conectando TCP|Enviando request HTTP' "$tmpdir/https.out"; then
  echo 'FAIL HTTPS caused a network phase'; exit 1
fi
echo 'PASS_SCOPED: host ELF plus actual binary HTTPS pre-network fail-closed'
python3 tests/loopback_http.py "$tmpdir/browserraf"
