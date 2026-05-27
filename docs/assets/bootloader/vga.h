#ifndef VGA_H
#define VGA_H

void vga_clear(void);
void vga_putc(char ch, unsigned char color);
void vga_puts(const char *str, unsigned char color);
void vga_put_hex8(unsigned char value);
void vga_put_hex32(unsigned int value);

#endif
