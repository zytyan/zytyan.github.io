#include "fat32.h"
#include "lib.h"
#include "linux_boot.h"
#include "vga.h"

static void halt(void)
{
	while (1) {
		__asm__ volatile("cli");
		__asm__ volatile("hlt");
	}
}

void protected_mode_C_entry()
{
	struct Fat32 fs;
	struct BootConfig cfg;
	struct Fat32File kernel;
	unsigned int entry = 0;
	unsigned int boot_params = 0;

	vga_clear();
	vga_puts("FAT32 boot config loader\n", 0x0f);
	debug_puts("FAT32 boot config loader\n");

	if (fat32_mount_boot_partition(&fs) != 0) {
		vga_puts("mount FAT32 partition: FAIL\n", 0x0c);
		debug_puts("mount FAT32 partition FAIL\n");
		halt();
	}

	vga_puts("mount FAT32 partition: OK part_lba=0x", 0x0a);
	vga_put_hex32((unsigned int)fs.part_lba);
	vga_putc('\n', 0x07);

	if (fat32_load_boot_config(&fs, &cfg) != 0) {
		vga_puts("read /BOOT.CFG: FAIL\n", 0x0c);
		debug_puts("read /BOOT.CFG FAIL\n");
		halt();
	}

	vga_puts("kernel: ", 0x07);
	vga_puts(cfg.kernel, 0x0f);
	vga_putc('\n', 0x07);

	vga_puts("cmdline: ", 0x07);
	vga_puts(cfg.cmdline[0] ? cfg.cmdline : "(empty)", 0x0f);
	vga_putc('\n', 0x07);

	debug_puts("kernel=");
	debug_puts(cfg.kernel);
	debug_puts("\ncmdline=");
	debug_puts(cfg.cmdline);
	debug_puts("\n");

	if (fat32_open(&fs, cfg.kernel, &kernel) != 0) {
		vga_puts("open kernel file: FAIL\n", 0x0c);
		debug_puts("open kernel file FAIL\n");
		halt();
	}

	vga_puts("open kernel file: OK size=0x", 0x0a);
	vga_put_hex32(kernel.size);
	vga_putc('\n', 0x07);
	debug_puts("open kernel file OK\n");

	vga_puts("load Linux image: ", 0x07);
	if (linux_load(&kernel, cfg.cmdline, &entry, &boot_params) != 0) {
		vga_puts("FAIL\n", 0x0c);
		debug_puts("load Linux image FAIL\n");
		halt();
	}

	vga_puts("OK entry=0x", 0x0a);
	vga_put_hex32(entry);
	vga_puts(" boot_params=0x", 0x0a);
	vga_put_hex32(boot_params);
	vga_putc('\n', 0x07);
	debug_puts("jumping to Linux\n");

	linux_boot32(entry, boot_params);
	halt();
}
