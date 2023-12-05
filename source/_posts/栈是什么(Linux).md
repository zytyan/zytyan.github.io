---
title: 基于Linux对栈的分析
date: 2023-09-10 19:13:24
tags: [堆栈, 操作系统, Linux, CPU]
---
本文以Linux系统为例，简析栈究竟是什么东西。其中的部分思想也可以用于其他操作系统。

# 栈是什么

函数运行的栈是计算机内存中的一种数据结构，用于管理函数调用和执行过程中的局部变量、参数、返回地址以及执行上下文。

但无论在什么情况下，栈都是一块内存。或许是一块物理内存，或许是通过mmap映射出来的虚拟内存，又或者是操作系统通过某种方式保留的内存。具体是什么需要依据操作系统和架构而定。

# 栈的内存分配原理



## 向上增长还是向下增长

有些教程和书中提到栈是向下增长的，堆是向上增长的，很多人不求甚解，就此记了下来，并且会根据猜测（说难听点就是臆想）去理解编译器怎么编译与程序怎么执行。实际上，栈的增长方向取决于CPU架构，而且仅仅是CPU架构中的一个小设定：加速栈存取的指令push会将SP增加还是减少。如果push减少SP，那么就是向下增长。

我们常用的x86架构的CPU，其push会减少rsp寄存器的值，所以其向下增长。而ARM架构的CPU，其并不自动维护栈，所以程序员完全可以选择编译向上增长的操作系统，而做到栈向上增长。类似的，IA64、MIPS架构则是默认向上增长。

## 为什么通常是向下增长

通过上面的解释，我们知道，栈增长方向并非什么金科玉律，其只是依赖于CPU架构而进行选择罢了。

而为什么我们最广泛使用的X86 CPU会使用向下增长的栈，这个我并没有查到具体的资料。 大体的说，这和早期的CPU与内存的设计有关，有人说是因为CPU要让程序知道有多少空间可以使用，也有人说是为了让程序总能以正数的偏移量访问其栈内部的数据。它并不是一个多么好或者多么坏的设计，向上增长或者向下增长不会对程序的易用性和安全性产生巨大的影响。而这么多年过去了，如此设计原因恐怕也只有其设计者才能知道了。

## 栈生长方向与栈帧布局

当然还是有人会把栈是向下生长的这种话作为金科玉律，这也是我写这篇文章的主要诱因。有些人不经过实测光凭想象就认为数组也是向下生长的，栈变量也是向下生长的。事实上，在函数内部，数组和栈变量如何布局与栈的生长方向完全没有关系，而是和编译器的实现有关系。是栈帧布局（stack frame layout）相关的内容了。但编译器的栈帧布局并不是我们主要讲的对象，所以这里我们只是做简单的说明。

对于数组而言，它并不是像CPU申请“我要一块栈空间”*n次，而是在函数编译时编译器就已经计算出来这个函数运行需要多少栈空间（这也是为什么C99前数组只能是定长的），并且在运行时提前为其分配对应的栈空间，也即将栈指针下移。

但无论栈是向下增长还是向上增长，数组的第N加一位的地址一定比数组的第N位地址要高。也就是说`&arr[n+1] > &arr[n]`恒为真，因为实际上`arr[n]`就是`*(arr + n)`的语法糖。而对于栈变量的布局，虽然大体上代码从前往后执行，编译器会从上往下排布栈变量，但是这也只是用于没有优化的阶段。如果开启了优化，栈变量连存在与否都不一定，更不要说内存的排布了。相信各位在使用gdb查看O2优化的程序的时候经常会看见`<optimized out>`，这就说明这个变量被优化没了。而对一些小的函数，其可能根本就没有使用到栈，而是仅仅在寄存器上就把所有的功能都完成并返回了，同样也没有栈变量与栈地址一说。



## 相关寄存器

### X86-64

#### rsp寄存器

stack pointer. 栈指针寄存器。这个寄存器标识了函数的栈顶在哪里，栈地址中高于这个寄存器的地址空间都是受保护的区域，中断处理程序不会对该区域进行修改。而当你使用`push/pop/call/ret/enter/leave`等指令时，它们会自动调整该寄存器的值。

特别的，在Linux系统中，其下方的128字节区域内依然是受保护的区域，这被称为red zone。这种处理主要是为了方便优化，使得一些比较小的叶函数（不调用其他函数的函数）不需要使用额外的指令去调整rsp的大小，而可以直接使用那128字节的保留区域。不过Windows没有这种保证，rsp下的栈空间是不稳定的。

#### rbp寄存器

base pointer. 用于保存栈底指针，但是在任何非零的优化等级且无VLA（可变长数组）的函数内，该寄存器都将被视为通用寄存器使用，并不保存栈底指针。因为一个不含VLA的函数，其所需要的空间大小是固定的。仅需要根据栈顶指针的值和固定大小的偏移量，即可算出栈底指针的值。这样可以节省一次访存操作以及几个指令的开销，同时对正常运行的程序不会造成任何影响，所以即使是最低等级的优化也会将该寄存器用于通用寄存器。

如果在函数中使用了VLA，则依然会保存栈底指针，因为此时函数需要的空间大小不确定。此时需要将原rbp压栈，并将当前的rsp赋值给rbp，然后根据VLA的长度调整rsp的大小。函数返回时将rbp赋值给rsp，然后从栈中取出原rbp，这通常使用`leave`指令实现。



### ARM64

Arm架构的CPU设计上相对来说就好多了，不再有那么多难记住的名字，取而代之的是所有的通用寄存器，都以Xn/Wn来命名。这让我们一眼就能看出哪些是通用寄存器，我们可以修改。

#### SP寄存器

类似于x86的rsp寄存器，同样有red zone保证。Apple产品会保证该区域不会被中断修改。

Linux 不知道。

我才不会去在ARM上玩Windows……

#### FP寄存器(X29)

Frame Pointer. 类似于x86的rbp寄存器，同样可用作通用寄存器。

#### LR寄存器(X30)

存储函数返回地址的寄存器，相较于X86 CPU，这是对叶子函数调用的进一步优化。它甚至不需要访问内存保存PC(rip)了，而是直接将PC转移到LR寄存器，能节省一些内存访问的开销。

## 为什么栈内存不设计为固定大小

猜测是因为需要支持在程序运行过程中动态调整栈的最大大小，所以没有将其在启动时就设定为固定大小。

但是除了主线程外，其他线程的栈为固定大小。

不同操作系统也有不同的做法，似乎Windows也会自动扩展，但总的来说，这不应该是一个可以假设的前提。

# 栈安全

## 与栈相关的漏洞

栈相关的漏洞较为基础，其有一些共同的特点：不正确的内存访问。这其中包括不正确的使用`scanf`, `(f)gets`,`str(mem)cpy`等函数，错误使用数组指针等。不过随着编译器的保护越来越多，静态代码检查越来越完善，很多可能产生漏洞的代码都会产生编译警告，而在生产环境下通常会使用`-Wall`选项将警告全部转换为错误强迫程序员修改该问题代码。

在计算机设计早期，内存页还不能设置执行权限的时候，栈内代码也可以执行。通过精心编排写入栈中的内容，可以做到任意代码执行，后来有了单独的执行权限，这种风险也就得以消除。

## 来自gcc的栈保护功能

然而，并非所有的风险都能被静态编译保护消除，仍然有许多风险不能在静态检查时被暴露。为此，gcc设计了栈保护器功能。该功能默认会在gcc认为可能产生漏洞的地方添加一层保护，无论优化级别如何，该选项始终会打开，可以通过`-fno-stack-protector`选项关闭检查。也可以通过`-fstack-protector-all`来对所有函数启用该检查，但需要注意的是该检查会堆运行时性能有一定影响，如果不是特别注重安全的情况下仅需默认栈保护即可。

### 使用Canary进行栈保护

gcc栈保护的原理是选取一个canary值，在函数运行时将其压入栈中，当函数即将返回时，对比栈中的canary的值与之前选取的canary值。如果不同则说明栈被破坏，将会调用`__stack_chk_fail`终止程序。

### Canary的选取

在X86-64架构Linux系统下，gcc11.4版本会选取`%fs:40`作为canary的值。在Linux下，fs寄存器被glibc用于存储指向TLS(Thread Local Storage)的指针，而其再偏移40字节后则是一段随机值。每个程序每次启动时都不一样，而由于溢出攻击的原理，攻击者往往不能得知该值的内容，所以能够有效检测里用缓冲区漏洞的攻击。

```C
/* from glibc    sysdeps/x86_64/nptl/tls.h   2023/09/10*/
typedef struct
{
  void *tcb;     /* 0-8 bits */	/* Pointer to the TCB.  Not necessarily the
			   thread descriptor used by libpthread.  */
  dtv_t *dtv;     /* 8-16 bits */
  void *self;     /* 16-24 bits */		/* Pointer to the thread descriptor.  */
  int multiple_threads;/* 24-28 bits */
  int gscope_flag;     /* 28-32 bits */
  uintptr_t sysinfo;   /* 32-40 bits */
  uintptr_t stack_guard;/* 40-48 bits <==> %fs:40 <==> %fs:0x28 */
  uintptr_t pointer_guard;
  unsigned long int unused_vgetcpu_cache[2];
  /* Bit 0: X86_FEATURE_1_IBT.
     Bit 1: X86_FEATURE_1_SHSTK.
   */
  unsigned int feature_1;
  int __glibc_unused1;
  /* Reservation of some values for the TM ABI.  */
  void *__private_tm[4];
  /* GCC split stack support.  */
  void *__private_ss;
  /* The lowest address of shadow stack,  */
  unsigned long long int ssp_base;
  /* Must be kept even if it is no longer used by glibc since programs,
     like AddressSanitizer, depend on the size of tcbhead_t.  */
  __128bits __glibc_unused2[8][4] __attribute__ ((aligned (32)));

  void *__padding[8];
} tcbhead_t;
```

不过这个值也并非所有位都是随机的，这和最常发生的栈溢出的原因有关：通常是不正确/不安全的字符串函数使用与意外/精心配置的错误字符串导致的。

除了写越界的任意代码执行很危险外，读越界导致栈中的敏感信息泄露同样很危险。所以另一种canary的思路是使用固定的字节组合`00 0D 0A FF (NUL CR LF EOF) (\0 \r \n \0xff)`作为Canary，虽然如今并没有看到其实际使用，但仍然为现在的栈保护提供了思路。使用它们作为canary时，虽然不能防止精心构造的写越界攻击，但是可以防止大多数的字符串函数读越界，字符串操作函数将会在遇到其中之一时停止。而glibc也利用了这一点，其`stack_guard`并非完全随机，在glibc的实现中，其低位总是0。这样可以缓解攻击者利用不正确的字符串处理函数获取canary后，再构造能特定于栈溢出攻击的串。

```C
/* sysdeps/generic/dl-osinfo.h#L22-L48 */
static inline uintptr_t __attribute__ ((always_inline))
_dl_setup_stack_chk_guard (void *dl_random)
{
  union
  {
    uintptr_t num;
    unsigned char bytes[sizeof (uintptr_t)];
  } ret = { 0 };

  if (dl_random == NULL)
    {
      ret.bytes[sizeof (ret) - 1] = 255;
      ret.bytes[sizeof (ret) - 2] = '\n';
    }
  else
    {
      memcpy (ret.bytes, dl_random, sizeof (ret));
// 区分大小端的宏，其目的就是让低位为 00 防止被str类函数读出内容
#if BYTE_ORDER == LITTLE_ENDIAN
      ret.num &= ~(uintptr_t) 0xff;
#elif BYTE_ORDER == BIG_ENDIAN
      ret.num &= ~((uintptr_t) 0xff << (8 * (sizeof (ret) - 1)));
#else
# error "BYTE_ORDER unknown"
#endif
    }
  return ret.num;
}
```



#### 不同系统、架构的区别

然而上面提到的canary仅面向X86-64 CPU。在arm架构上，这种保护有着不同的实现方式。其值被静态编译进二进制文件当中，而非从TLS中读取。 Windows上的msvc同样也支持类似原理的栈保护功能。



## 操作系统中针对栈的保护

操作系统中也有针对栈溢出相应的保护。与编译器相比，操作系统提供的保护是对整个栈空间的保护。虽然实现原理上各不相同，但本质都是不允许栈顶的一部分空间可读写，以此在栈溢出时让程序直接停止工作，而不是出现内存数据泄露或任意代码执行的风险。

### 栈保护页／栈保护空洞

以Linux内核的实现为例。在栈向下生长时内核总是额外要求1MB的空间，虽然额外的空间不会被映射，但是当检查时如果没有这1MB空间的空余，那么则会直接返回`-ENOMEM`，程序segmentation fault。

```C
// mm/mmap.c  @expand_downwards
/* Enforce stack_guard_gap */
int expand_downwards(struct vm_area_struct *vma, unsigned long address) {
    // ...
    prev = mas_prev(&mas, 0);
    /* Check that both stack segments have the same anon_vma? */
    if (prev) {
        if (!(prev->vm_flags & VM_GROWSDOWN) &&
            vma_is_accessible(prev) &&
            (address - prev->vm_end < stack_guard_gap))
            return -ENOMEM;
    }

    if (prev)
        mas_next_range(&mas, vma->vm_start);
    // ...
}
/* enforced gap between the expanding stack and other mappings. */
// 4KB page size，256*page_size = 1MB
unsigned long stack_guard_gap = 256UL<<PAGE_SHIFT; // #define PAGE_SHIFT 12 

```

Windows则是以不可读、不可写、不可执行的页面放在栈顶来保护栈溢出的情况。

## 地址随机化

现在操作系统使用地址随机化来缓解和地址有关的攻击。在没有地址随机化之前，函数的栈、堆、系统调用等映射均是固定的。其他的库的映射也相对较为固定。有潜在的被利用风险。 Linux内核早在2.6版本就通过引入地址随机化的方式来缓解这一攻击风险。简单地说就是让这些固定位置的映射的地址都有一定随机程度的起始位置偏移，使得攻击者更难预测需要利用的数据的位置。这缓解了一系列任意代码执行漏洞及内存敏感信息泄露漏洞等风险。

### 为什么生产中不应该打印函数指针

让外界知道运行时的函数指针是一个非常危险的行为。如果打印了某个函数指针，则代表该动态链接库、或者该程序内的其他指针都有可能被计算出来。尤其是对于一些公共库的函数，更应该注意。因为攻击者不一定能获取到内部的二进制文件，但公共的二进制文件，比如glibc，是很容易获取到的。一旦该库中的函数指针被打印，那么就很容易计算出该库中的其他函数指针的位置，地址随机化就没有缓解攻击的功能了，此时配合缓冲区溢出漏洞，可以构造出return to glibc攻击，而由于glibc中含有非常多可以用于攻击的实用函数，故该行为非常危险。

