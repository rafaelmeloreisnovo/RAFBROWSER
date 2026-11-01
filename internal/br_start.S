/* br_start.S — Entry ARM32/ARM64/x86-64
 *
 * Designated bootstrap module for the process boundary only.  The fixed
 * VECTRA register contract remains reserved for kernel/hot-path modules;
 * this file documents its bootstrap exception at each ABI entry below.
 */
#if defined(__arm__)
.syntax unified
.thumb
.text
.align 2
.global _start
.global browser_main
.thumb_func

/* BR_BOOTSTRAP_ENTER_BROWSER_MAIN
 * Ponto de entrada permitido: única transição documentada _start ->
 * browser_main neste bootstrap ARM32.  The branch target is the linked C
 * entry point, not an arbitrary/unknown symbol.
 *
 * Register contract:
 * - Before transition: r11/lr are cleared for the bootstrap frame boundary;
 *   r0..r4 are not used by this bootstrap before browser_main.
 * - After return: r0 is written only as the immediate exit_group(0) status;
 *   r1..r4 remain untouched by this bootstrap.
 */
.macro BR_BOOTSTRAP_ENTER_BROWSER_MAIN
    bl   browser_main
.endm

_start:
    mov  r11,#0
    mov  lr,#0
    BR_BOOTSTRAP_ENTER_BROWSER_MAIN
    mov  r7,#248
    mov  r0,#0
    svc  #0
.h: b .h
#elif defined(__aarch64__)
.text
.align 4
.global _start
.global browser_main

/* BR_BOOTSTRAP_ENTER_BROWSER_MAIN
 * Ponto de entrada permitido: única transição documentada _start ->
 * browser_main neste bootstrap AArch64.  This macro is the designated
 * bootstrap hand-off and the only local place allowed to link to the C
 * entry point.
 *
 * Register contract:
 * - Before transition: x29/x30 are cleared and sp is aligned to 16 bytes;
 *   x0..x4 are not used by this bootstrap before browser_main.
 * - After return: x0 is written only as the immediate exit_group(0) status;
 *   x1..x4 remain untouched by this bootstrap, and x8 holds syscall #94.
 */
.macro BR_BOOTSTRAP_ENTER_BROWSER_MAIN
    bl   browser_main
.endm

_start:
    mov  x29,xzr
    mov  x30,xzr
    /* ELF AArch64 process-entry ABI provides 16-byte-aligned SP; do not use SP as AND destination. */
    BR_BOOTSTRAP_ENTER_BROWSER_MAIN
    mov  x0,xzr
    mov  x8,#94
    svc  #0
.h: b .h
#elif defined(__x86_64__)
.text
.globl _start
.globl browser_main
_start:
    xor  %rbp,%rbp
    call browser_main
    mov  $231,%rax
    xor  %rdi,%rdi
    syscall
#endif
#if defined(__arm__) || defined(__aarch64__)
.section .note.GNU-stack,"",%progbits
#else
.section .note.GNU-stack,"",@progbits
#endif
