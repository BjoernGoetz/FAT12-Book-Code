#!/bin/bash
gcc \
  -m32 \
  -ffreestanding \
  -nostdlib \
  -no-pie \
  -Wall \
  -Wextra \
  -iquote include \
  -iquote src \
  -T kernel.ld \
  -o kernel.elf \
  src/*.c \
  platform/kernel/*.c \
  platform/kernel/startup.s
