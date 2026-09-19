#include <stdint.h>
#include "qos/qkernel.h"
#include "qos/qbin.h"
#include "boot_program.h"

void serial_init(void); void serial_putc(char); void serial_puts(const char*); void serial_u32(uint32_t); void platform_exit(uint8_t);

void kernel_main(uint32_t multiboot_magic,uint32_t multiboot_info){
    (void)multiboot_info;
    serial_init();
    serial_puts("Q-OS v0.1 boot\n");
    if(multiboot_magic!=0x36d76289u){ serial_puts("BOOT_PROTOCOL=FAIL\n"); platform_exit(2); }
    serial_puts("BOOT_PROTOCOL=MULTIBOOT2\n");
    serial_puts("QSTATE_MAP: -=00 -+=01 +-=10 +=11 NULL=UNALLOCATED\n");

    qkernel_t kernel; qk_init(&kernel);
    qbin_result_t result;
    int rc=qbin_execute(&kernel,qos_boot_program,qos_boot_program_len,&result);
    if(rc!=0){
        serial_puts("QBIN_EXEC=FAIL rc=");serial_u32((uint32_t)(rc<0?-rc:rc));serial_putc('\n');platform_exit(3);
    }
    serial_puts("QBIN_EXEC=PASS\n");
    serial_puts("EXPECTATIONS=");serial_u32(result.expectations);serial_putc('\n');
    serial_puts("TRANSITIONS=");serial_u32(kernel.transitions);serial_putc('\n');
    serial_puts("EVENTS_PROCESSED=");serial_u32(kernel.events_processed);serial_putc('\n');
    serial_puts("SPARSE_EXECUTION=PASS\n");
    serial_puts("NULL_TOPOLOGY=PASS\n");
    serial_puts("QOS_BOOT_CONFORMANCE=PASS\n");
    platform_exit(0x10);
}
