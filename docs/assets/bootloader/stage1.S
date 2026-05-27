.code16
.section .stage1, "ax", @progbits

.global _start
.global read_disk_lba_16
.global puts16
.global boot_driver_id

.extern into_32_flat_mode

_start:
  cli  /* 关闭中断，防止BIOS中断打断代码 */
  xorw %ax, %ax
  movw %ax, %ds   /* 清空DS基址寄存器 */
  movw %ax, %es   /* 清空ES基址寄存器 */
  movw %ax, %ss
  movw $0x7C00, %SP /* 设置栈顶，避免覆盖代码 */
  movb %DL, boot_driver_id /* 保存启动时的驱动器号 */
  sti
  call load_next_code
  movw $info_into_32_flat_mode, %AX
  call puts16
  call into_32_flat_mode

load_next_code:
  /* 从磁盘加载所有代码，我们假设读8KB */
  /* 开一块栈空间存储DAP结构体 */
  pushw %BP
  pushw %ds
  pushw %es
  movw %SP, %BP
  andw $0xFFF0, %SP  /* 16字节对齐 */
  subw $16, %SP
  movw %sp, %si  /* DAP结构体地址放在SI寄存器中 */
  /* 设置DAP结构体 */
  movb $0x10, (%si)       /* DAP size */
  movb $0, 1(%si)      /* reserved */
  movw read_sector_count, %ax
  movw %ax, 2(%si)
  movw $(0x7C00+512), 4(%si) /* offset: 0x7C00 + 512 */
  movw $0, 6(%si) /* segment: 0x0000 */
  movl $1, 8(%si) /* lba: 1 */
  movl $0, 12(%si) /* lba高32位 */
  movb boot_driver_id, %dl /* 驱动器号放在DL寄存器中 */
  call read_disk_lba_16
  jc .Ldisk_read_error
  movw %bp, %sp
  popw %es
  popw %ds
  popw %bp
  movw $info_load_success, %AX
  call puts16
  ret
.Ldisk_read_error:
  movw $info_disk_read_error, %AX
  call panic

puts16:
  pushw %BX
  pushw %SI
  movw %AX, %SI
  movb $0x07, %BL
.Lpubs16_start_putc:
  movb (%SI), %AL
  testb %AL, %AL
  jz .Lputs16_end
#ifndef NON_QEMU_DEBUG
  outb %AL, $0xE9 /* 向QEMU的调试控制台输出 */
#endif
  movb $0x0E, %AH
  int $0x10
  inc %SI
  jmp .Lpubs16_start_putc
.Lputs16_end:
  popw %SI
  popw %BX
  ret

panic:
    testw %AX, %AX
    jz .Lpanic_halt
    call puts16
    cli
.Lpanic_halt:
    hlt
    jmp .Lpanic_halt

read_disk_lba_16:
    /* 用BIOS的LBA读硬盘
        调用约定： 驱动器号:DL, DAP地址: DS:SI
            - 成功： CF=0, AX=0
            - 失败： CF=1, AX=错误码

        struct DAP {
            uint8_t  size;       // 0x10
            uint8_t  reserved;   // 0
            uint16_t count;      // 要读的扇区数
            uint16_t offset;     // 目标 offset
            uint16_t segment;    // 目标 segment
            uint64_t lba;        // 起始 LBA
        };
    */
    movb $0x42, %AH  /* BIOS 扩展磁盘服务 - 读扇区 */
    int $0x13
    ret

info_into_32_flat_mode:
  .asciz "Prepare to enter 32-bit protected mode\r\n"
info_disk_read_error:
  .asciz "Disk read error! Halting.\r\n"
info_load_success:
  .asciz "Disk read successful! Jumping to protected mode...\r\n"

.org 442
read_sector_count:
  .word 0
boot_driver_id:
  .byte 0x00 /* 从启动时的寄存器中获取驱动器号 */
