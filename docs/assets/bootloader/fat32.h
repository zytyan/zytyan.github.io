#ifndef FAT32_H
#define FAT32_H

#include "disk.h"

#define FAT32_MAX_PATH 128
#define FAT32_MAX_CMDLINE 256
#define FAT32_MAX_CFG 512

struct Fat32 {
	uint64_t part_lba;
	uint32_t fat_lba;
	uint32_t data_lba;
	uint32_t root_cluster;
	uint32_t sectors_per_fat;
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint8_t fat_count;
};

struct Fat32File {
	struct Fat32 *fs;
	uint32_t first_cluster;
	uint32_t size;
};

struct BootConfig {
	char kernel[FAT32_MAX_PATH];
	char cmdline[FAT32_MAX_CMDLINE];
};

int fat32_mount_boot_partition(struct Fat32 *fs);
int fat32_open(struct Fat32 *fs, const char *path, struct Fat32File *file);
int fat32_read_file(struct Fat32File *file, void *buf, uint32_t buf_size,
		    uint32_t *bytes_read);
int fat32_read_at(struct Fat32File *file, uint32_t offset, void *buf,
		  uint32_t size, uint32_t *bytes_read);
int fat32_load_boot_config(struct Fat32 *fs, struct BootConfig *cfg);

#endif // FAT32_H
