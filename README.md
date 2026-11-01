# RAFBROWSER — freestanding terminal HTTP browser

**Code state:** built from the BrowserRaf source snapshot `585d1b2abd0622b886e12f306b27882ac6f07eb4` (2026-11-01).  
**This repository contains the executable source, build/test scripts, and CI.** It is no longer a documentation-only pointer.

## What is implemented

- C/Assembly, fixed-buffer HTTP/1.1 text browser using Linux/Android user-space syscalls.
- Bounded URL parsing, IPv4 literals and DNS A lookup, GET, exact Content-Length and strict chunked framing.
- Text/HTML rendering, sanitized links, interactive selection and volatile back navigation.
- Same-origin path redirects with a hop limit; non-text responses and unsupported schemes fail closed.
- Cross-ABI builds and CI checks for x86-64, ARMv7, and AArch64; offline loopback and malformed-input tests.
- Pixel-kernel code and tests are included as an adjacent freestanding module; they do not decode image formats or drive browser rendering.

## Build and verify

Requires Clang; QEMU user mode is additionally required for ARM emulation.

```sh
make
./build/browserraf http://127.0.0.1:8080/
./build/browserraf http://127.0.0.1:8080/ --interactive
make test
```

For a no-root Termux build/safety smoke:

```sh
sh tests/check_termux_device.sh
```

The device script does not claim HTTP behavior on a physical Android device unless its receipt says so.

## Security and product boundary

HTTPS is deliberately rejected before DNS/network access. The TLS header is not a trusted TLS client. DNS currently uses a hardcoded plaintext resolver; HTTP content and endpoint identity are unauthenticated. Do not send credentials or sensitive content. This is not a WebView/Chromium-class browser and does not provide JavaScript, DOM/CSS layout, cookies, forms, WebSockets, downloads, or persistence.

These limits are code-level constraints, not documentation-only caveats. A Linux/QEMU pass is not physical Android evidence or a security certification.

## Source relationship

The runtime and test files were copied byte-for-byte from [BrowserRaf](https://github.com/rafaelmeloreisnovo/BrowserRaf) at `585d1b2abd0622b886e12f306b27882ac6f07eb4`. Source blob hashes and the update boundary are in [SOURCE_LINEAGE.md](docs/SOURCE_LINEAGE.md).

The repositories now carry independent copies. Changes in BrowserRaf do not automatically change RAFBROWSER; every later import must update the pinned source SHA and preserve the blob-level receipt.