#include "disk.h"
#include "lib.h"

#define BIOS_DAP_ADDR 0x500
#define BIOS_BUF_ADDR 0x10000
#define BIOS_BUF_SECTORS 127
#define SECTOR_SIZE 512

extern int bios_read_disk_32(struct DAP *dap);
static int read_disk_by_bios(struct DAP *dap);

static void *fixed_phys_ptr(uint32_t addr)
{
    asm volatile("" : "+r"(addr));
    return (void *)addr;
}

int read_disk_lba(uint64_t lba, uint16_t count, void *buf)
{
    struct DAP dap = {
        .size = 0x10,
        .reserved = 0,
        .count = count,
        .offset = (uint16_t)((uint32_t)buf & 0xf),
        .segment = (uint16_t)((uint32_t)buf >> 4),
        .lba = lba,
    };

    return read_disk_by_bios(&dap);
}

static int read_disk_by_bios(struct DAP *dap)
{
    // 由于X86内存寻址限制，BIOS一次最多只能读127个扇区，因此需要分批读取
    struct DAP *bios_dap = fixed_phys_ptr(BIOS_DAP_ADDR);
    void *target = (void *)((uint32_t)dap->segment << 4 | dap->offset);
    uint16_t remaining = dap->count;
    uint64_t lba = dap->lba;
    char *out = target;

    while (remaining) {
        uint16_t count = remaining > BIOS_BUF_SECTORS ? BIOS_BUF_SECTORS : remaining;

        *bios_dap = *dap;
        bios_dap->count = count;
        bios_dap->offset = BIOS_BUF_ADDR & 0xf;
        bios_dap->segment = BIOS_BUF_ADDR >> 4;
        bios_dap->lba = lba;

        if (bios_read_disk_32(bios_dap) != 0)
            return -1;

        memcpy(out, (void *)BIOS_BUF_ADDR, (intptr_t)count * SECTOR_SIZE);
        out += (uint32_t)count * SECTOR_SIZE;
        lba += count;
        remaining -= count;
    }

    return 0;
}
