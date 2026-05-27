#ifndef DISK_H
#define DISK_H

#include "lib.h"

struct DAP {
	uint8_t size; // 0x10
	uint8_t reserved; // 0
	uint16_t count; // 要读的扇区数
	uint16_t offset; // 目标 offset
	uint16_t segment; // 目标 segment
	uint64_t lba; // 起始 LBA
};

int read_disk_lba(uint64_t lba, uint16_t count, void *buf);

#endif // DISK_H
