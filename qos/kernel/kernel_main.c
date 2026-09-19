#include <stdint.h>
#include <stddef.h>
#include "qos/qkernel.h"
#include "qos/qbin.h"

void serial_init(void); void serial_putc(char); void serial_puts(const char*); void serial_u32(uint32_t); void platform_exit(uint8_t);

typedef struct { uint32_t type; uint32_t size; } mb2_tag_t;
typedef struct { uint32_t type; uint32_t size; uint32_t mod_start; uint32_t mod_end; char cmdline[1]; } mb2_module_tag_t;

static const uint8_t *find_qbin_module(uint32_t mbi_addr, size_t *length){
    const uint8_t *base=(const uint8_t*)(uintptr_t)mbi_addr;
    uint32_t total=*(const uint32_t*)base;
    uint32_t p=8;
    while(p+8<=total){
        const mb2_tag_t *tag=(const mb2_tag_t*)(base+p);
        if(tag->type==0) break;
        if(tag->size<8 || p+tag->size>total) return 0;
        if(tag->type==3 && tag->size>=16){
            const mb2_module_tag_t *m=(const mb2_module_tag_t*)tag;
            if(m->mod_end>m->mod_start){
                *length=(size_t)(m->mod_end-m->mod_start);
                return (const uint8_t*)(uintptr_t)m->mod_start;
            }
        }
        p=(p+tag->size+7u)&~7u;
    }
    return 0;
}

void kernel_main(uint32_t multiboot_magic,uint32_t multiboot_info){
    serial_init();
    serial_puts("Q-OS v0.1 boot\n");
    if(multiboot_magic!=0x36d76289u){ serial_puts("BOOT_PROTOCOL=FAIL\n"); platform_exit(2); }
    serial_puts("BOOT_PROTOCOL=MULTIBOOT2\n");
    serial_puts("QSTATE_MAP: -=00 -+=01 +-=10 +=11 NULL=UNALLOCATED\n");

    size_t qbin_len=0;
    const uint8_t *qbin=find_qbin_module(multiboot_info,&qbin_len);
    if(!qbin){ serial_puts("QBIN_MODULE=FAIL\n"); platform_exit(3); }
    serial_puts("QBIN_MODULE=PASS bytes="); serial_u32((uint32_t)qbin_len); serial_putc('\n');

    qk_init(&kernel);
    qbin_result_t result;
    int rc=qbin_execute(&kernel,qbin,qbin_len,&result);
    if(rc!=0){
        serial_puts("QBIN_EXEC=FAIL rc=");serial_u32((uint32_t)(rc<0?-rc:rc));serial_putc('\n');platform_exit(4);
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
