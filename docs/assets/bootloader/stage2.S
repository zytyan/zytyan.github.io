.code16
.section .text, "ax", @progbits

.global into_32_flat_mode
.global bios_read_disk_32
.global linux_boot32

.extern boot_driver_id
.extern puts16
.extern debug_puts
.extern protected_mode_C_entry

.equ CODE32_SEL, 0x10
.equ DATA32_SEL, 0x18
.equ CODE16_SEL, 0x20
.equ DATA16_SEL, 0x28

.macro flat_desc access, flags
    .word 0xffff
    .word 0x0
    .byte 0x0
    .byte \access
    .byte \flags
    .byte 0x0
.endm

into_32_flat_mode:
  /* 关中断 */
  cli
  /* 开启 A20 总线 (使用 Fast A20 端口 0x92) */
  inb     $0x92, %al
  orb     $2, %al
  outb    %al, $0x92
  movw $info_a20_enable, %AX
  call puts16
  /* 加载GDT */
  lgdt gdt_pointer
  /* 开启保护模式 */
  movl    %cr0, %eax
  orl     $1, %eax
  movl    %eax, %cr0
  /* 远跳转，刷新CS寄存器 */
  ljmpl $CODE32_SEL, $protected_mode_asm_entry

bios_pm16_entry:
  movw $DATA16_SEL, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss
  movl %cr0, %eax
  andl $0xfffffffe, %eax
  movl %eax, %cr0
  ljmp $0x0000, $bios_real_entry

bios_real_entry:
  xorw %ax, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss
  movw $0x7c00, %sp
  movw bios_dap_ptr, %si
  movb boot_driver_id, %dl
  movb $0x42, %ah
  int $0x13
  jc .Lbios_read_error
  xorw %ax, %ax
  jmp .Lbios_read_done
.Lbios_read_error:
  movb %ah, %al
  xorb %ah, %ah
.Lbios_read_done:
  movw %ax, bios_status
  cli
  lgdt gdt_pointer
  movl %cr0, %eax
  orl $1, %eax
  movl %eax, %cr0
  ljmpl $CODE32_SEL, $bios_pm32_return

.code32
bios_read_disk_32:
  pushl %ebp
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi
  movl %esp, bios_saved_esp
  movl 8(%ebp), %eax
  movl %eax, bios_dap_ptr
  cli
  ljmp $CODE16_SEL, $bios_pm16_entry

bios_pm32_return:
  movw $DATA32_SEL, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss
  movl bios_saved_esp, %esp
  movzwl bios_status, %eax
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret

linux_boot32:
  movl 4(%esp), %eax
  movl 8(%esp), %esi
  cli
  lgdt gdt_pointer
  movw $DATA32_SEL, %dx
  movw %dx, %ds
  movw %dx, %es
  movw %dx, %fs
  movw %dx, %gs
  movw %dx, %ss
  xorl %ebp, %ebp
  xorl %edi, %edi
  xorl %ebx, %ebx
  pushl $CODE32_SEL
  pushl %eax
  lret

protected_mode_asm_entry:
  /* 现在已经进入保护模式了，设置段寄存器 */
  movw $DATA32_SEL, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %fs
  movw %ax, %gs
  movw %ax, %ss
  movl $0x70000, %esp /* 设置栈顶，避免被1MiB处的Linux内核覆盖 */
  pushl $info_setup_segments
  call debug_puts
  popl %ecx
  /* 跳转到 C 语言入口点，注意：现在还不能开中断 */
  pushl $info_into_C_entry
  call debug_puts
  popl %ecx

  cld
  call protected_mode_C_entry
  cli
.Lhang:
  hlt
  jmp .Lhang
.p2align 3                      # 8 字节对齐
gdt_start:
    # 空描述符 (8字节全0)
    .quad 0x0

    # 0x08 reserved, so Linux can use __BOOT_CS=0x10 and __BOOT_DS=0x18.
    .quad 0x0

    # 0x10: 4G flat RX, used by bootloader and Linux __BOOT_CS.
    flat_desc 0x9a, 0xcf

    # 0x18: 4G flat RW, used by bootloader and Linux __BOOT_DS.
    flat_desc 0x92, 0xcf

    # 0x20/0x28: 16-bit protected-mode code/data for BIOS calls.
    flat_desc 0x9a, 0x00
    flat_desc 0x92, 0x00
gdt_end:

info_a20_enable:
  .asciz "Enabling A20 line...\r\n"
info_setup_segments:
  .asciz "Setting up GDT and entering protected mode...\r\n"
info_into_C_entry:
  .asciz "Jumping to C entry point in protected mode...\r\n"

# GDT 描述符指针 (传递给 lgdt)
gdt_pointer:
    .word gdt_end - gdt_start - 1 # GDT 界限 (大小 - 1)
    .long gdt_start               # GDT 32位线性基地址

.section .data, "aw", @progbits
bios_saved_esp:
    .long 0
bios_dap_ptr:
    .long 0
bios_status:
    .word 0
