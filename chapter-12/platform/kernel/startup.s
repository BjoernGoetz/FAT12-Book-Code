# Multiboot header: the magic number the bootloader scans for, flags
# requesting no features, and a checksum that makes all three sum
# to zero
.section .multiboot
.long 0x1BADB002
.long 0
.long -(0x1BADB002 + 0)

# Entry point: execution starts at _start
.global _start

# Code section
.section .text

# The loader jumps here: point ESP at our stack before any C code
# runs, then hand off to kernel_main in C
_start:
    mov $stack_top, %esp
    call kernel_main

# kernel_main returned, so nothing is left to run: disable interrupts
# and halt the CPU, halting again if a non-maskable interrupt (one
# that cli can't block) wakes it anyway
halt:
    cli
    hlt
    jmp halt

# Uninitialized data section
.section .bss

# Space for a 16 KiB stack for our C code, growing down from
# stack_top toward stack_bottom, 16-byte aligned as C expects
.align 16
stack_bottom:
.skip 16384
stack_top:
