#include <stdint.h>
static inline void outb(uint16_t port,uint8_t value){ __asm__ volatile("outb %0,%1"::"a"(value),"Nd"(port)); }
static inline uint8_t inb(uint16_t port){ uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(port)); return r; }
void serial_init(void){
    outb(0x3F8+1,0x00); outb(0x3F8+3,0x80); outb(0x3F8+0,0x03); outb(0x3F8+1,0x00);
    outb(0x3F8+3,0x03); outb(0x3F8+2,0xC7); outb(0x3F8+4,0x0B);
}
void serial_putc(char c){ while((inb(0x3F8+5)&0x20)==0){} outb(0x3F8,(uint8_t)c); }
void serial_puts(const char *s){ while(*s) serial_putc(*s++); }
void serial_u32(uint32_t x){ char b[11]; unsigned n=0; if(!x){serial_putc('0');return;} while(x){b[n++]=(char)('0'+x%10);x/=10;} while(n)serial_putc(b[--n]); }
void platform_exit(uint8_t code){ outb(0xF4,code); for(;;)__asm__ volatile("hlt"); }
