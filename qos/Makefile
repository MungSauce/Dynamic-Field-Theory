CC ?= gcc
LD ?= ld
PY ?= python3
CFLAGS_HOST=-std=c11 -O2 -Wall -Wextra -pedantic -Iinclude
CFLAGS_KERNEL=-m32 -std=c11 -O2 -Wall -Wextra -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Iinclude -Ikernel
ASFLAGS=-m32 -ffreestanding -fno-pie -nostdlib
LDFLAGS=-m elf_i386 -T kernel/linker.ld -nostdlib
BUILD=build

.PHONY: all test kernel clean qasm
all: test kernel

$(BUILD):
	mkdir -p $(BUILD)

qasm: | $(BUILD)
	$(PY) tools/qasmc.py examples/boot.qasm kernel/boot_program.h --c-header

test: | $(BUILD)
	$(CC) $(CFLAGS_HOST) kernel/qkernel.c kernel/qbin.c tests/test_qkernel.c -o $(BUILD)/test_qkernel
	$(BUILD)/test_qkernel
	$(CC) $(CFLAGS_HOST) kernel/qkernel.c kernel/qbin.c tests/test_qbin.c -o $(BUILD)/test_qbin
	$(BUILD)/test_qbin
	$(PY) tools/qasmc.py examples/boot.qasm $(BUILD)/boot.qbin

kernel: qasm | $(BUILD)
	$(CC) $(ASFLAGS) -c kernel/boot.S -o $(BUILD)/boot.o
	$(CC) $(CFLAGS_KERNEL) -c kernel/qkernel.c -o $(BUILD)/qkernel.o
	$(CC) $(CFLAGS_KERNEL) -c kernel/qbin.c -o $(BUILD)/qbin.o
	$(CC) $(CFLAGS_KERNEL) -c kernel/platform_x86.c -o $(BUILD)/platform_x86.o
	$(CC) $(CFLAGS_KERNEL) -c kernel/kernel_main.c -o $(BUILD)/kernel_main.o
	$(LD) $(LDFLAGS) -o $(BUILD)/qos-kernel.elf $(BUILD)/boot.o $(BUILD)/qkernel.o $(BUILD)/qbin.o $(BUILD)/platform_x86.o $(BUILD)/kernel_main.o

clean:
	rm -rf $(BUILD)
