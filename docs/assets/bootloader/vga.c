#include "vga.h"

static volatile unsigned short *vram = (unsigned short *)0x000B8000;
static int vga_pos;

void vga_clear(void)
{
  for (int i = 0; i < 80 * 25; i++)
    vram[i] = (0x07 << 8) | ' ';
  vga_pos = 0;
}

void vga_putc(char ch, unsigned char color)
{
  if (ch == '\n') {
    vga_pos += 80 - vga_pos % 80;
    return;
  }

  vram[vga_pos++] = (color << 8) | ch;
}

void vga_puts(const char *str, unsigned char color)
{
  while (*str)
    vga_putc(*str++, color);
}

void vga_put_hex8(unsigned char value)
{
  static const char hex[] = "0123456789ABCDEF";
  vga_putc(hex[value >> 4], 0x0f);
  vga_putc(hex[value & 0x0f], 0x0f);
}

void vga_put_hex32(unsigned int value)
{
  for (int shift = 28; shift >= 0; shift -= 4) {
    unsigned char nibble = (value >> shift) & 0x0f;
    vga_putc(nibble < 10 ? '0' + nibble : 'A' + nibble - 10, 0x0f);
  }
}
