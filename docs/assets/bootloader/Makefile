BUILD_DIR := _build

AS := as
CC := gcc
LD := ld
OBJCOPY := objcopy

ASFLAGS := --32 -g
CFLAGS := -Wall -Wextra -O2 -m32 -fno-builtin -fno-pic -fno-pie -c -g
DEPFLAGS := -MMD -MP
LDFLAGS := -m elf_i386 -T boot.ld

ASM_SRCS := stage1.S stage2.S
C_SRCS := lib.c vga.c disk.c fat32.c linux_boot.c main.c
OBJS := \
	$(BUILD_DIR)/stage1.o \
	$(BUILD_DIR)/stage2.o \
	$(BUILD_DIR)/lib.o \
	$(BUILD_DIR)/vga.o \
	$(BUILD_DIR)/disk.o \
	$(BUILD_DIR)/fat32.o \
	$(BUILD_DIR)/linux_boot.o \
	$(BUILD_DIR)/main.o
DEPS := $(OBJS:.o=.d)

.PHONY: build clean rebuild

build: boot.bin

rebuild: clean build

boot.bin: boot.elf
	$(OBJCOPY) -O binary $< $@
	sector_count=$$((($$(stat -c '%s' $@) + 511) / 512 - 1)); \
	perl -e 'print pack("v", shift)' "$$sector_count" | dd of=$@ bs=1 seek=442 conv=notrunc status=none

boot.elf: $(OBJS) boot.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR) boot.elf boot.bin

-include $(DEPS)
