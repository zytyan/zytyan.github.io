#include "linux_boot.h"
#include "lib.h"

#define BOOT_PARAMS_ADDR 0x90000
#define CMDLINE_ADDR 0x9e000
#define KERNEL_LOAD_ADDR 0x100000
#define BOOT_PARAMS_SIZE 4096
#define KERNEL_HEADER_SIZE 4096

#define SETUP_HDR_OFFSET 0x1f1
#define SETUP_HDR_END_OFFSET 0x201
#define HDR_MAGIC_OFFSET 0x202
#define VERSION_OFFSET 0x206
#define TYPE_OF_LOADER_OFFSET 0x210
#define LOADFLAGS_OFFSET 0x211
#define CODE32_START_OFFSET 0x214
#define HEAP_END_PTR_OFFSET 0x224
#define CMD_LINE_PTR_OFFSET 0x228
#define CMDLINE_SIZE_OFFSET 0x238
#define E820_COUNT_OFFSET 0x1e8
#define E820_TABLE_OFFSET 0x2d0

#define LOADED_HIGH 0x01
#define CAN_USE_HEAP 0x80
#define E820_TYPE_RAM 1

static unsigned char kernel_header[KERNEL_HEADER_SIZE];

static void add_e820(unsigned char *boot_params, int index,
		     unsigned long long addr, unsigned long long size,
		     unsigned int type)
{
	unsigned char *ent = boot_params + E820_TABLE_OFFSET + index * 20;

	wr64(ent, addr);
	wr64(ent + 8, size);
	wr32(ent + 16, type);
}

static void copy_cmdline(const char *src, unsigned int limit)
{
	char *dst = (char *)CMDLINE_ADDR;
	unsigned int i = 0;

	if (limit == 0)
		limit = 255;

	while (src[i] && i + 1 < limit) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

int linux_load(struct Fat32File *kernel, const char *cmdline,
	       unsigned int *entry, unsigned int *boot_params_addr)
{
	unsigned char *boot_params = (unsigned char *)BOOT_PARAMS_ADDR;
	unsigned int bytes = 0;
	unsigned int setup_sects;
	unsigned int setup_bytes;
	unsigned int protocol;
	unsigned int header_end;
	unsigned int header_bytes;
	unsigned int cmdline_limit;
	unsigned int kernel_bytes;

	if (fat32_read_at(kernel, 0, kernel_header, sizeof(kernel_header),
			  &bytes) != 0 ||
	    bytes < 0x240)
		return -1;

	if (rd16(kernel_header + 0x1fe) != 0xaa55)
		return -1;
	if (rd32(kernel_header + HDR_MAGIC_OFFSET) != 0x53726448)
		return -1;

	protocol = rd16(kernel_header + VERSION_OFFSET);
	if (protocol < 0x0202)
		return -1;
	if ((kernel_header[LOADFLAGS_OFFSET] & LOADED_HIGH) == 0)
		return -1;

	setup_sects = kernel_header[SETUP_HDR_OFFSET];
	if (setup_sects == 0)
		setup_sects = 4;
	setup_bytes = (setup_sects + 1) * 512;
	if (setup_bytes >= kernel->size)
		return -1;

	header_end = HDR_MAGIC_OFFSET + kernel_header[SETUP_HDR_END_OFFSET];
	if (header_end < 0x240 || header_end > 0x290)
		header_end = 0x240;
	header_bytes = header_end - SETUP_HDR_OFFSET;

	memset(boot_params, 0, BOOT_PARAMS_SIZE);
	memcpy(boot_params + SETUP_HDR_OFFSET, kernel_header + SETUP_HDR_OFFSET,
	       header_bytes);

	boot_params[TYPE_OF_LOADER_OFFSET] = 0xff;
	boot_params[LOADFLAGS_OFFSET] |= CAN_USE_HEAP;
	wr16(boot_params + HEAP_END_PTR_OFFSET, 0xde00);
	wr32(boot_params + CMD_LINE_PTR_OFFSET, CMDLINE_ADDR);
	wr32(boot_params + CODE32_START_OFFSET, KERNEL_LOAD_ADDR);

	cmdline_limit = rd32(boot_params + CMDLINE_SIZE_OFFSET);
	if (cmdline_limit == 0 || cmdline_limit > 4096)
		cmdline_limit = 255;
	copy_cmdline(cmdline, cmdline_limit);

	boot_params[E820_COUNT_OFFSET] = 2;
	add_e820(boot_params, 0, 0x00000000ULL, 0x0009fc00ULL, E820_TYPE_RAM);
	add_e820(boot_params, 1, 0x00100000ULL, 0x03f00000ULL, E820_TYPE_RAM);

	kernel_bytes = kernel->size - setup_bytes;
	if (fat32_read_at(kernel, setup_bytes, (void *)KERNEL_LOAD_ADDR,
			  kernel_bytes, &bytes) != 0 ||
	    bytes != kernel_bytes)
		return -1;

	*entry = KERNEL_LOAD_ADDR;
	*boot_params_addr = BOOT_PARAMS_ADDR;
	return 0;
}
