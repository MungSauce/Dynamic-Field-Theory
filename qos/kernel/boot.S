.set MB2_MAGIC, 0xe85250d6
.set MB2_ARCH, 0
.section .multiboot2
.align 8
header_start:
.long MB2_MAGIC
.long MB2_ARCH
.long header_end - header_start
.long -(MB2_MAGIC + MB2_ARCH + (header_end - header_start))
.short 0
.short 0
.long 8
header_end:

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start,@function
_start:
    cli
    mov $stack_top, %esp
    push %ebx
    push %eax
    call kernel_main
1:  hlt
    jmp 1b
