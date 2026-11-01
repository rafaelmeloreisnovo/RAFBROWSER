#!/bin/sh
# Termux (Android, no root): local device smoke and reproducible receipt.
# Only run in a checked-out BrowserRaf tree ON the physical device.
# No network, credentials, external programs except native build tools.
set -eu
cd "$(dirname "$0")/.."
command -v clang >/dev/null 2>&1 || { echo 'TOKEN_VAZIO_MISSING_NATIVE_CLANG'; exit 2; }
command -v grep >/dev/null 2>&1 || { echo 'TOKEN_VAZIO_MISSING_GREP'; exit 2; }
readelf_bin=""
if command -v llvm-readelf >/dev/null 2>&1; then readelf_bin=llvm-readelf
elif command -v readelf >/dev/null 2>&1; then readelf_bin=readelf
else echo 'TOKEN_VAZIO_MISSING_READELF'; exit 2
fi
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
clang -O2 -ffreestanding -fno-builtin -fno-stack-protector \
  -fPIE -pie -nostdlib -fuse-ld=lld \
  -Wl,-e,_start -Wl,--build-id=none -Wl,-z,noexecstack \
  -Iinternal internal/br_start.S internal/br_main.c -o "$tmp/browserraf"
"$readelf_bin" -h "$tmp/browserraf" > "$tmp/elf.txt"
if "$readelf_bin" -l "$tmp/browserraf" | grep -q INTERP; then
  echo 'FAIL_ANDROID_ELF_INTERP'; exit 1
fi
if "$readelf_bin" -d "$tmp/browserraf" 2>/dev/null | grep -q NEEDED; then
  echo 'FAIL_ANDROID_ELF_NEEDED'; exit 1
fi
"$tmp/browserraf" https://127.0.0.1/ > "$tmp/out.txt" 2>&1
grep -Fq 'HTTPS não suportado neste build' "$tmp/out.txt" ||
  { echo 'FAIL_ANDROID_HTTPS_NOT_DENIED'; exit 1; }
if grep -E 'Resolvendo DNS|Conectando TCP' "$tmp/out.txt"; then
  echo 'FAIL_ANDROID_UNSAFE_NETWORK'; exit 1
fi
echo 'PASS_DEVICE_SCOPED native Android/Termux binary executed; HTTPS blocked before network'
uname -m
if command -v getprop >/dev/null 2>&1; then
  getprop ro.product.model
  getprop ro.build.version.sdk
fi
"$readelf_bin" -h "$tmp/browserraf" | grep -E 'Class:|Machine:|Type:'
if command -v sha256sum >/dev/null 2>&1; then sha256sum "$tmp/browserraf"; fi
echo 'NOT_TESTED local real HTTP/DNS, user interaction, TLS trust, full browser compatibility'
