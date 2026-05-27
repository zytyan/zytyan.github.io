#ifndef LINUX_BOOT_H
#define LINUX_BOOT_H

#include "fat32.h"

int linux_load(struct Fat32File *kernel, const char *cmdline,
	       unsigned int *entry, unsigned int *boot_params);
void linux_boot32(unsigned int entry, unsigned int boot_params);

#endif // LINUX_BOOT_H
