---
title: ELF文件详解（1）
date: 2025-07-29 21:33:16
tags: [Linux, ELF, C]
---

# 什么是ELF

ELF，全称**E**xecutable and **L**inkable **F**ormat，即可执行可链接格式，是Unix首次提出的一种二进制接口标准，如今已经成为Unix与Linux世界中最重要的文件格式。这些系统中，二进制可执行文件、动态链接库、核心转储、部分编译中间产物均为该格式。

通过学习ELF格式，我们可以了解前人是如何设计通用且高性能的二进制文件格式的，并且这些知识可以帮助我们更快、更好地定位常见C、C++程序中遇到的问题。

若无特殊说明，本文章中涉及的程序X86-64架构，大部分C代码都可以在其他平台上编译。

## 前期准备

为了更简单地分析ELF文件，我们需要一个Linux环境，并安装`gcc`、`readelf`和`objdump`，`gcc`用于将示例C代码编译为二进制程序，`readelf`和`objdump`用于分析ELF文件。Linux系统没有具体要求，无论是物理机、虚拟机还是WSL都可以，如果您是一位有经验的用户，您也可以选择在Windows上分析。下面以Debian/Ubuntu为例。

```bash
# 安装readelf与objdump
sudo apt install binutils
# 安装gcc及相关工具链
sudo apt install build-essential
```

# 从第一个程序开始

让我们从第一个C语言程序开始，看看它编译出的文件是什么样子的。

```C
#include <stdio.h>
int main() {
    printf("Hello World!");  // 这里不加\n，否则有个优化会导致实际调用的并非printf
    return 0;
}
```

将文件保存为`hello_world.c`，使用`gcc hello_world.c -o hello_world`来进行编译。

我们知道，一个ELF文件本质上与其他文件并无不同，它们都是硬盘中的一串二进制数据。真正让系统和工具识别ELF文件的是它的各种**头**。文件头就如同一本书的封面和目录，通过一些固定的约定，让这本书即使打印在顺序的纸张上，也能展示出结构化的特征。

## ELF文件的三种头

### ELF头

ELF头就好像书的封面，它总在文件的最前端，Linux内核通过读取该头，来判断其是否是ELF文件。每个ELF文件都有且仅有一个ELF头，可以使用`readelf -h`查看ELF头中所包含的内容。

```
qxy@mannix:~/test_c$ readelf -h hello_world
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              DYN (Position-Independent Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0x1060
  Start of program headers:          64 (bytes into file)
  Start of section headers:          13984 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           56 (bytes)
  Number of program headers:         13
  Size of section headers:           64 (bytes)
  Number of section headers:         31
  Section header string table index: 30
```

让我们从头开始，解析这段信息的内容。

- Magic：7f 45 4c 46 ... ，这是ELF文件的**魔术头**，这样，即使没有后缀，内核依然可以通过魔术头来识别一个ELF文件。后面的45 4c 46 正好是ASCII码的大写的ELF三个字母。
- Class 为 ELF64 表明这是一个64位 ELF 文件。而Data则描述了使用的数据格式是补码，小端序。
  ELF支持多种不同的数据格式，为此，需要在头中描述自己所使用的是哪种格式，以便工具可以正确解析。
- 从 OS/ABI 到 Version 描述了这个文件适用的系统、架构，Version是该文件所遵循的ELF标准的版本号，目前固定为1。
- Entry point address指明了这个文件的入口点为0x1060，反汇编后会发现这里正好是`_start`函数所在位置。
- Flags在X86平台没有什么用处，通常为0。部分平台会在这里放入一些有意义的标志位，用来指示 ABI、浮点支持、指令集等。
- 剩下的与header有关的字段则是本文的重点，包括程序头、段头与ELF头。

### 节头

ELF 文件中的 **section（节）** 是一种逻辑上的数据划分单位，用来标注文件中不同区域的用途。每个节都有自己的名称和属性，用于辅助编译器、调试器等工具对文件进行分析。这些节被整合在了ELF的特定位置（通常是尾部），被称为**节头表**。类似于书的目录中记录了每个章节的大致内容和它所在的页码。节头表中的节头则标识了ELF文件中不同区域的名称、类型、映射后的虚拟地址、和各种其他属性。

节是面试中的常客，通常八股所谓的代码在text“段”，初始化的数据在data“段”，未初始化的数据在bss“段”，说的就是这些数据位于对应的节（section），将其翻译为段，更多的是一种误译，后面的程序头中的segment更适合翻译为“段”。

一个ELF通常有十几到三十多个节，但也有像`libc`这样多达六十几个节的文件。使用`readelf -SW hello_world`来读取程序的**节头表**，通过节头表，可以看到程序具体有哪些节。不过，节头只对编译器、调试器等分析工具有帮助，真正执行时是不需要节头的。如同一本书可以没有目录，一个完整可执行的ELF文件也可以完全节头表。

> `readelf`中的`-W`参数告诉程序不要截断输出。不使用该选项会导致一些比较长的内容可能会被截断，有些内容会还被分为两行输出，这对屏幕很宽的现代设备并不友好。

```
qxy@mannix:~/test_c$ readelf -SW hello_world
There are 31 section headers, starting at offset 0x36b8:

Section Headers:
  [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
  [ 1] .interp           PROGBITS        0000000000000318 000318 00001c 00   A  0   0  1
  [ 2] .note.gnu.property NOTE           0000000000000338 000338 000030 00   A  0   0  8
  [ 3] .note.gnu.build-id NOTE           0000000000000368 000368 000024 00   A  0   0  4
  [ 4] .note.ABI-tag     NOTE            000000000000038c 00038c 000020 00   A  0   0  4
  [ 5] .gnu.hash         GNU_HASH        00000000000003b0 0003b0 000024 00   A  6   0  8
  [ 6] .dynsym           DYNSYM          00000000000003d8 0003d8 0000a8 18   A  7   1  8
  [ 7] .dynstr           STRTAB          0000000000000480 000480 00008f 00   A  0   0  1
  [ 8] .gnu.version      VERSYM          0000000000000510 000510 00000e 02   A  6   0  2
  [ 9] .gnu.version_r    VERNEED         0000000000000520 000520 000030 00   A  7   1  8
  [10] .rela.dyn         RELA            0000000000000550 000550 0000c0 18   A  6   0  8
  [11] .rela.plt         RELA            0000000000000610 000610 000018 18  AI  6  24  8
  [12] .init             PROGBITS        0000000000001000 001000 00001b 00  AX  0   0  4
  [13] .plt              PROGBITS        0000000000001020 001020 000020 10  AX  0   0 16
  [14] .plt.got          PROGBITS        0000000000001040 001040 000010 10  AX  0   0 16
  [15] .plt.sec          PROGBITS        0000000000001050 001050 000010 10  AX  0   0 16
  [16] .text             PROGBITS        0000000000001060 001060 00010c 00  AX  0   0 16
  [17] .fini             PROGBITS        000000000000116c 00116c 00000d 00  AX  0   0  4
  [18] .rodata           PROGBITS        0000000000002000 002000 000011 00   A  0   0  4
  [19] .eh_frame_hdr     PROGBITS        0000000000002014 002014 000034 00   A  0   0  4
  [20] .eh_frame         PROGBITS        0000000000002048 002048 0000ac 00   A  0   0  8
  [21] .init_array       INIT_ARRAY      0000000000003db8 002db8 000008 08  WA  0   0  8
  [22] .fini_array       FINI_ARRAY      0000000000003dc0 002dc0 000008 08  WA  0   0  8
  [23] .dynamic          DYNAMIC         0000000000003dc8 002dc8 0001f0 10  WA  7   0  8
  [24] .got              PROGBITS        0000000000003fb8 002fb8 000048 08  WA  0   0  8
  [25] .data             PROGBITS        0000000000004000 003000 000010 00  WA  0   0  8
  [26] .bss              NOBITS          0000000000004020 003010 009c60 00  WA  0   0 32
  [27] .comment          PROGBITS        0000000000000000 003010 00002b 01  MS  0   0  1
  [28] .symtab           SYMTAB          0000000000000000 003040 000378 18     29  18  8
  [29] .strtab           STRTAB          0000000000000000 0033b8 0001e3 00      0   0  1
  [30] .shstrtab         STRTAB          0000000000000000 00359b 00011a 00      0   0  1
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
  L (link order), O (extra OS processing required), G (group), T (TLS),
  C (compressed), x (unknown), o (OS specific), E (exclude),
  D (mbind), l (large), p (processor specific)
```



可以看到，在`hello_world`这个简单的编译结果中，有多达30个节。这些节会被编译、调试工具（如我们正在使用的`readelf`、`gdb`等）所使用，用于解析内容、支撑调试。但是在程序运行时是不需要节头表的。即使一个ELF文件完全没有节头，也可以正常运行。

一个节头中包含多种信息，下面是这些头的简单解释。

| 列名    | 含义                                                         |
| ------- | ------------------------------------------------------------ |
| Nr      | 节编号（Section Number）                                     |
| Name    | 节的名称（如 `.text`、`.data` 等）                           |
| Type    | 节的类型（如 `PROGBITS`、`NOBITS`、`NOTE` 等）               |
| Address | 节加载到内存中的地址（虚拟地址）                             |
| Off     | 节在文件中的偏移（Offset）                                   |
| Size    | 节的大小（以字节为单位）                                     |
| ES      | 每个条目的大小（Entry Size），比如 `.symtab` 中每个符号的大小 |
| Flg     | 标志（Flags），例如：`W` 可写，`A` 可加载到内存，`X` 可执行  |
| Lk      | 链接项（Link），具体含义依节的类型而不同，例如符号表关联的字符串节 |
| Inf     | 附加信息（Info），具体含义也依节类型而定                     |
| Al      | 对齐（Alignment），节在内存中的对齐要求，通常为 4、8、16 等  |

当然，对节来说，最重要的是每个节具体有什么作用。这里我按照节的作用，分为以下几类。

| 节名                                                         | 功能                                                         |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| `.text`                                                      | 代码节，这里是真正用于执行的机器指令。                       |
| `.data` `.bss` `.rodata`                                     | 数据节，其中的ro代表Read Only。                              |
| `.init` `.fini` `.init_array` `.fini_array`                  | 用于存放程序的初始化与清理代码                               |
| `.plt` `.got` `.plt.*` `.rela.*` `.dynstr` `.dynsym` `.gnu.hash` | 重定位相关，运行时寻找外部库符号，或其他库寻找本库符号时需要使用这些节。运行时重定位可以说是整个动态链接过程中最复杂的。 |
| `.eh_frame_hdr` `.eh_frame`                                  | 异常相关的节，用于C++的异常处理功能。                        |
| `.dynamic`                                                   | 用于动态库的加载，是整个动态库的核心部分。                   |
| `.comment` `.symtab` `.strtab` `.note.*`                     | 通常是一些注释节，对运行没有任何作用。                       |
| `.shstrtab `                                                 | 节头表，打印节头信息就是通过读取它实现的。                   |



### 程序头

程序头是ELF在运行时的核心。在上面的节头中，我们看到节头包含了很多信息，但内核和链接器并不需要这么多的信息。并且，节头的名称是以字符串的形式存储的，这会让需要争分夺秒的内核和动态链接器浪费很多时间在处理字符串上。为了让程序更快地运行，ELF文件还有对机器更友好的程序头表（program header table）。通过`readelf -l`可以读取程序头表中的信息。

```
qxy@mannix:~/test_c$ readelf -lW hello_world

Elf file type is DYN (Position-Independent Executable file)
Entry point 0x1060
There are 13 program headers, starting at offset 64

Program Headers:
  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
  PHDR           0x000040 0x0000000000000040 0x0000000000000040 0x0002d8 0x0002d8 R   0x8
  INTERP         0x000318 0x0000000000000318 0x0000000000000318 0x00001c 0x00001c R   0x1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x000628 0x000628 R   0x1000
  LOAD           0x001000 0x0000000000001000 0x0000000000001000 0x000179 0x000179 R E 0x1000
  LOAD           0x002000 0x0000000000002000 0x0000000000002000 0x0000f4 0x0000f4 R   0x1000
  LOAD           0x002db8 0x0000000000003db8 0x0000000000003db8 0x000258 0x009ec8 RW  0x1000
  DYNAMIC        0x002dc8 0x0000000000003dc8 0x0000000000003dc8 0x0001f0 0x0001f0 RW  0x8
  NOTE           0x000338 0x0000000000000338 0x0000000000000338 0x000030 0x000030 R   0x8
  NOTE           0x000368 0x0000000000000368 0x0000000000000368 0x000044 0x000044 R   0x4
  GNU_PROPERTY   0x000338 0x0000000000000338 0x0000000000000338 0x000030 0x000030 R   0x8
  GNU_EH_FRAME   0x002014 0x0000000000002014 0x0000000000002014 0x000034 0x000034 R   0x4
  GNU_STACK      0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 RW  0x10
  GNU_RELRO      0x002db8 0x0000000000003db8 0x0000000000003db8 0x000248 0x000248 R   0x1

 Section to Segment mapping:
  Segment Sections...
   00
   01     .interp
   02     .interp .note.gnu.property .note.gnu.build-id .note.ABI-tag .gnu.hash .dynsym .dynstr .gnu.version .gnu.version_r .rela.dyn .rela.plt
   03     .init .plt .plt.got .plt.sec .text .fini
   04     .rodata .eh_frame_hdr .eh_frame
   05     .init_array .fini_array .dynamic .got .data .bss
   06     .dynamic
   07     .note.gnu.property
   08     .note.gnu.build-id .note.ABI-tag
   09     .note.gnu.property
   10     .eh_frame_hdr
   11
   12     .init_array .fini_array .dynamic .got
```

在`readelf`的输出中，上方是程序头表中的程序头，下方则是这些程序头包含的节。与节头不同，程序头的Type（LOAD、NOTE等）部分，并非字符串，而是提前预设的宏值，这避免了字符串操作带来的时间开销。

可以看到，相较于包含三十多个节头的节头表，程序头表就少多了，只有12个。事实上，运行一个程序，内核所需要的信息比我们上方看到的还要少一些。内核只需要知道一个文件的内存如何布局（映射部分、动态分配部分及其权限、对齐），使用哪种动态链接器（或者不使用）以及入口点，就可以执行一个程序了。所以上面的程序头对内核来说，真正必须的部分只有`INTERP`、`LOAD`两个部分。如果是一个静态链接程序，连`INTERP`部分也不需要。

> 如果您注意力惊人，您应该可以注意到`.shstrtab`并没有程序头表中出现，这也就意味着 存储着节名的`.shstrtab`并非让程序运行的必要信息。

> 在计算机发展的早期，CPU的页式内存管理和RWX权限模型尚不完整，栈通常是可执行的。可执行的栈带来了一系列安全问题，`GNU_STACK`头就是用于解决该问题的机制，通过显式告知内核栈不可执行来提供更好的安全保护。类似地，内核也会尝试读取`GNU_PROPERTY`头来为ELF启用或停用一些特定功能。但它们并非必须。



当然，大多数程序都是动态链接程序，虽然内核并不需要其他部分，但`ld-linux-x86-64.so.2`这个动态链接器是需要额外信息来加载库和修改部分内存权限。例如，`GNU_EH_FRAME`用于C++的异常处理相关，`DYNAMIC`用于让动态链接器加载其必须信息。

