ENTRY(_start)

PHDRS
{
  text PT_LOAD FLAGS(5);
  rodata PT_LOAD FLAGS(4);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0x7c00;
  .boot : {
    *(.stage1)
  } :text
  ASSERT(SIZEOF(.boot) <= 446, "Error: stage1 section exceeds 512 bytes!")
  . = 0x7c00 + 510;
  .mbr : {
    BYTE(0x55)
    BYTE(0xAA)
  } :text
  . = 0x7c00+512;
  .read_disk_test_magic_number : ALIGN(1) {
    BYTE(0xDE)
    BYTE(0xAC)
  } :text
  .text ALIGN(4): {
    *(.text*)
  } :text
  .rodata ALIGN(4): {
    *(.rodata*)
  } :rodata
  .data ALIGN(4): {
    *(.data*)
  } :data
  .bss ALIGN(4): {
    *(.bss*)
    *(COMMON)
  } :data
  /DISCARD/ : {
    *(.eh_frame*)
  }

}
