# RAFBROWSER

> Project entry point — state checked 2026-11-01

This repository currently has no source files or commit history available to inspect. The name identifies a browser project, but no implementation contract, target platform, or source lineage is present here yet. This README establishes a low-friction route without fabricating those missing details.

## Current source of implementation

The existing browser implementation is maintained in [BrowserRaf](https://github.com/rafaelmeloreisnovo/BrowserRaf). Its observed main commit on 2026-11-01 is `a27b8c78db21a149523c858387b11ff4ccce8c7c`.

BrowserRaf is an experimental freestanding Linux/Android user-space HTTP/1.1 text browser. It deliberately rejects HTTPS because trusted TLS is not implemented. It is not a Chromium/WebView-class browser. See its [current entry route](https://github.com/rafaelmeloreisnovo/BrowserRaf/blob/main/START_HERE.md) and [dated state receipt](https://github.com/rafaelmeloreisnovo/BrowserRaf/blob/main/docs/CURRENT_STATE_2026-11-01.md).

## Repository boundary

Until this repository gains a source contract and an execution target, BrowserRaf remains the single source of browser implementation. Do not copy its C/ASM core here: duplicated production code would create two heads, two test histories, and unclear ownership.

This repository is currently a navigation seed only. That is an observed repository state, not an implementation or runtime claim.

## Route to implementation

1. Read BrowserRaf's `START_HERE.md` for capabilities, build instructions, safety limits, and exact-head evidence.
2. Treat the product-readiness table as the acceptance backlog; preserve `TOKEN_VAZIO` where device or security evidence is absent.
3. Make code changes in the implementation repository unless a separate RAFBROWSER target contract is established.
4. Keep source, artifact, execution, evidence, and claim distinct.
5. A Linux/QEMU pass is not an Android-device pass; HTTP support is not trusted HTTPS.

## Completion state

- RAFBROWSER: initialized with this repository route; no browser source code added.
- Browser implementation: see the linked BrowserRaf state receipt.
- Promotion/release: not implied by repository initialization.
- Next evidence gate: full freestanding-core CI on BrowserRaf's current candidate head, followed by physical Android evidence and independent security/license review.
