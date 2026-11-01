#!/bin/sh
# Reproducible S7 host color vectors + ARM32/ARM64 no-libc ELF link.
set -eu
command -v clang >/dev/null 2>&1 || { echo MISSING_CLANG; exit 2; }
command -v readelf >/dev/null 2>&1 || { echo MISSING_READELF; exit 2; }
cd "$(dirname "$0")/.."
d="$(mktemp -d)"
trap 'rm -rf "$d"' EXIT HUP INT TERM
clang -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer tests/test_pixel_matrix.c -o "$d/pixel-test"
"$d/pixel-test"
cat >"$d/pixel_probe.c" <<'C_EOF'
#include "br_pixel.h"
static BRPixStats stats;
static const brpix_u8 rgb[3]={0,0,255};
void _start(void){
  volatile int ret=brpix_scan_rgb24(rgb,3u,1u,1u,3u,1u,20u,&stats);
  (void)ret;
  for(;;){}
}
C_EOF
for target in armv7a-linux-androideabi21 aarch64-linux-android28; do
  case "$target" in
    armv7a-*) machine=ARM ;;
    aarch64-*) machine=AArch64 ;;
    *) exit 2 ;;
  esac
  clang --target="$target" -std=c11 -O2 -fuse-ld=lld -ffreestanding \
      -fno-builtin -fno-stack-protector -nostdlib -static \
      -Wl,-e,_start -Wl,--build-id=none -Wl,-z,noexecstack \
      -Iinternal "$d/pixel_probe.c" -o "$d/$target.elf"
  readelf -h "$d/$target.elf" | grep -q "Machine:.*$machine"
  if readelf -l "$d/$target.elf" | grep -q INTERP;then echo FAIL_INTERP;exit 1;fi
  if readelf -d "$d/$target.elf" 2>/dev/null | grep -q NEEDED;then echo FAIL_NEEDED;exit 1;fi
  if readelf -W -s "$d/$target.elf" | grep ' UND ' | grep -v 'NOTYPE  LOCAL  DEFAULT  UND ';then
    echo FAIL_UNDEFINED_SYMBOL;exit 1
  fi
  echo "PASS_PIXEL_ABI_LINK_ONLY $target"
done
