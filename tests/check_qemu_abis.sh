#!/bin/sh
# Test harness only; QEMU is NOT a production dependency or physical Android proof.
set -eu
cd "$(dirname "$0")/.."
for tool in clang qemu-arm qemu-aarch64 python3 readelf; do
  command -v "$tool" >/dev/null 2>&1 || { echo "MISSING_TEST_TOOL $tool"; exit 2; }
done
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for spec in "armv7a-linux-androideabi21 qemu-arm ARM" "aarch64-linux-android28 qemu-aarch64 AArch64"; do
  set -- $spec
  triple="$1"; emulator="$2"; machine="$3"
  clang --target="$triple" -O2 -ffreestanding -fno-builtin \
    -fno-stack-protector -fno-pie -fno-pic -nostdlib -static \
    -fuse-ld=lld -Wl,-e,_start -Wl,--build-id=none \
    -Wl,-z,noexecstack -Iinternal \
    internal/br_start.S internal/br_main.c -o "$tmp/$triple-browser"
  readelf -h "$tmp/$triple-browser" | grep -q "Machine:.*$machine"
  # Safe runtime proof: HTTPS is denied before socket/DNS even under emulation.
  "$emulator" "$tmp/$triple-browser" https://127.0.0.1/ >"$tmp/$triple-https.log" 2>&1
  grep -Fq 'HTTPS não suportado neste build' "$tmp/$triple-https.log" ||
      { echo "FAIL emulated HTTPS denial $triple"; cat "$tmp/$triple-https.log"; exit 1; }
  if grep -E 'Resolvendo DNS|Conectando TCP' "$tmp/$triple-https.log"; then
    echo "FAIL emulator reached network $triple"; exit 1
  fi
  # Python wrapper preserves the existing local-only HTTP integration suite.
  printf '#!/bin/sh\nexec %s "%s" "$@"\n' "$emulator" "$tmp/$triple-browser" > "$tmp/$triple-run"
  chmod +x "$tmp/$triple-run"
  python3 tests/loopback_http.py "$tmp/$triple-run"
  echo "PASS_EMULATED_ABI $triple: static ELF/HTTPS refusal/loopback text browsing"
done
