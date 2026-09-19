# Q-OS

Q-OS is a temporal-vector operating system whose native allocated state is one of `-`, `-+`, `+-`, `+`; `Null` is structural absence.

This directory contains the first bootable reference kernel, Q-ASM compiler, QBIN loader, packed quaternary memory, sparse event scheduler, and conformance tests.

Build hosted tests:

`make test`

Build the freestanding kernel:

`make kernel`

The GitHub Actions workflow additionally constructs a GRUB ISO, boots it under QEMU, and requires `QOS_BOOT_CONFORMANCE=PASS` from the independent kernel.
