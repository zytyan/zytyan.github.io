---
title: EMOCK工作原理
date: 2024-06-23 09:54:50
tags: [EMOCK, C, C++, 汇编]

---

# EMOCK 简介

首先来简单介绍一下EMOCK，EMOCK是C和C++语言常见的MOCK库，它可以在运行时将可执行程序或动态链接库的代码替换为mock函数，以实现对函数进行打桩。其原理为修改函数入口处的代码，将其替换为跳转到mock函数的代码。

例如实际业务代码中有获取1-100随机值的函数，但在测试中我们只希望验证随机值为1的情况，那么就可以使用EMOCK。

```C++
// get_random.c at libcredit.so
__attribute__((noinline))int get_random()
{
    return rand() % 100;
}

int get_credit()
{
    if (get_random() == 1) {
        return 100;
    }
    return 10;
}
// end

// test.cpp
#include "gtest/gtest.h"
#include "emock/emock.h"
extern "C"{
int get_random();
int get_credit();
}
TEST(CreditTest, should_get_100_credit_when_random_is_1)
{
    // 这里我们只希望获取 `get_random` 返回1的情况
    // 但是so中的函数已经被编译，无法修改，此时需要使用EMOCK
    // 通过下面的代码，可以让 `get_random` 函数永远返回1
    EMOCK(get_random).stubs().with(any()).will(returnValue(1));
    ASSERT_EQ(get_credit(), 100);
}
```

# EMOCK 工作原理

## 前言：从零开始设计MOCK

如果不限制任何条件，重新从零开始设计一个C/C++ MOCK工具，应该怎么设计呢？

首先想到的最简单的方式就是直接修改被测函数的源代码，为其增加一个全局的hook函数指针，当hook不为空时就返回hook函数。在测试代码中使用 `extern` 关键字获取该变量并赋值

```C++
int (*_hook_get_random)()  = NULL;
int get_random()
{
    if (_hook_get_random) {
        return _hook_get_random();
    }
	return rand() % 100;    
}
// test.cpp

extern "C" {
extern int (*_hook_get_random)()
}
int get_random_as_1()
{
    return 1;
}
TEST(BaseTest, should_xxx)
{
    _hook_get_random = get_random_as_1;
}
```

通过使用宏，还可以避免在生产环境运行时测试代码的不必要占用。

```C
#ifdef _BUILDING_TEST
int (*_hook_get_random)()  = NULL;
#endif
int get_random()
{
#ifdef _BUILDING_TEST
    // 只有测试代码才会包含这个检查，上线运行时不会有额外的开销。
    if (_hook_get_random) {
        return _hook_get_random();
    }
#endif
	return rand() % 100;    
}

```



这种方式虽然可行，但缺陷显然也非常明显，每一个被测函数都需要为其准备一个钩子变量，并且要在函数中插入对该钩子变量的检测，虽然生产环境没有额外的性能开销，但是显然增加了不少编码的负担。同时，生产环境的代码几乎必须要使用第三方库，为第三方库增加大量的这种测试代码不仅不符合常理，也容易因为失误引起不稳定。更何况大多数第三方库是预编译的动态链接库，不可能为已经编译好的库插入这种代码。

那么究竟该如何做一个通用的mock呢？

## CPU的运行方式：以X86-64为例

虽然在逻辑上，存在[仅需一条指令就可以图灵完备的CPU](https://en.wikipedia.org/wiki/One-instruction_set_computer)，但实际上一个可用的CPU至少应该有三种指令：算术指令，包括整数、浮点、位运算、寄存器传递；访存指令，即读取/写入内存的指令；跳转指令，包括相对跳转、绝对跳转，这其中还可以分为有条件或无条件跳转。

现在有如下简单的的C语言代码，为了演示方便，这段代码的编译不使用任何优化（`-O0`）。

```C
#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int main() {
    int a = 4;
    int b = 5;
    int res;
    if (a == 4) {
        res = add(a, b);
        goto end;
    } else {
        res = sub(a, b);
    }
    printf("Oops, I can't be executed!\n");
end:
    printf("sum = %d\n", res);
    return 0;
}
```

这段代码在我的设备上编译后，使用`objdump -d`获得编译后文件的人类可读格式，这里我们忽略不是我们编写的函数。

```assembly
Disassembly of section .plt.sec:

0000000000001050 <printf@plt>:
    1050:       f3 0f 1e fa             endbr64
    1054:       f2 ff 25 75 2f 00 00    bnd jmp *0x2f75(%rip)        # 3fd0 <printf@GLIBC_2.2.5>
    105b:       0f 1f 44 00 00          nopl   0x0(%rax,%rax,1)

Disassembly of section .text:

......
0000000000001169 <add>:
    1169:       f3 0f 1e fa             endbr64
    116d:       55                      push   %rbp
    116e:       48 89 e5                mov    %rsp,%rbp
    1171:       89 7d fc                mov    %edi,-0x4(%rbp)
    1174:       89 75 f8                mov    %esi,-0x8(%rbp)
    1177:       8b 55 fc                mov    -0x4(%rbp),%edx
    117a:       8b 45 f8                mov    -0x8(%rbp),%eax
    117d:       01 d0                   add    %edx,%eax
    117f:       5d                      pop    %rbp
    1180:       c3                      ret

0000000000001181 <sub>:
    1181:       f3 0f 1e fa             endbr64
    1185:       55                      push   %rbp
    1186:       48 89 e5                mov    %rsp,%rbp
    1189:       89 7d fc                mov    %edi,-0x4(%rbp)
    118c:       89 75 f8                mov    %esi,-0x8(%rbp)
    118f:       8b 45 fc                mov    -0x4(%rbp),%eax
    1192:       2b 45 f8                sub    -0x8(%rbp),%eax
    1195:       5d                      pop    %rbp
    1196:       c3                      ret

0000000000001197 <main>:
    1197:       f3 0f 1e fa             endbr64
    119b:       55                      push   %rbp
    119c:       48 89 e5                mov    %rsp,%rbp
    119f:       48 83 ec 10             sub    $0x10,%rsp
    11a3:       c7 45 f8 04 00 00 00    movl   $0x4,-0x8(%rbp)
    11aa:       c7 45 fc 05 00 00 00    movl   $0x5,-0x4(%rbp)
    11b1:       83 7d f8 04             cmpl   $0x4,-0x8(%rbp)
    11b5:       75 14                   jne    11cb <main+0x34>
    11b7:       8b 55 fc                mov    -0x4(%rbp),%edx
    11ba:       8b 45 f8                mov    -0x8(%rbp),%eax
    11bd:       89 d6                   mov    %edx,%esi
    11bf:       89 c7                   mov    %eax,%edi
    11c1:       e8 a3 ff ff ff          call   1169 <add>
    11c6:       89 45 f4                mov    %eax,-0xc(%rbp)
    11c9:       eb 21                   jmp    11ec <main+0x55>
    11cb:       8b 55 fc                mov    -0x4(%rbp),%edx
    11ce:       8b 45 f8                mov    -0x8(%rbp),%eax
    11d1:       89 d6                   mov    %edx,%esi
    11d3:       89 c7                   mov    %eax,%edi
    11d5:       e8 a7 ff ff ff          call   1181 <sub>
    11da:       89 45 f4                mov    %eax,-0xc(%rbp)
    11dd:       48 8d 05 20 0e 00 00    lea    0xe20(%rip),%rax        # 2004 <_IO_stdin_used+0x4>
    11e4:       48 89 c7                mov    %rax,%rdi
    11e7:       e8 74 fe ff ff          call   1060 <puts@plt>
    11ec:       8b 45 f4                mov    -0xc(%rbp),%eax
    11ef:       89 c6                   mov    %eax,%esi
    11f1:       48 8d 05 27 0e 00 00    lea    0xe27(%rip),%rax        # 201f <_IO_stdin_used+0x1f>
    11f8:       48 89 c7                mov    %rax,%rdi
    11fb:       b8 00 00 00 00          mov    $0x0,%eax
    1200:       e8 6b fe ff ff          call   1070 <printf@plt>
    1205:       b8 00 00 00 00          mov    $0x0,%eax
    120a:       c9                      leave
    120b:       c3                      ret

```

在objdump的输出中，第一列为以十六进制表示的文件中的偏移量，第二列为机器代码的16进制表现形式，第三列为反汇编出来的汇编代码，与机器代码一一对应。CPU就是通过一条条执行第二列的二进制机器代码来运行程序的，如果改变这些机器代码，就可以改变程序的执行流程，而这里就是可以mock的地方。

## 运行时修改机器代码

想要修改，首先需要确定被修改的位置。如果要将某个函数替换为mock函数，直觉上可以修改每个调用该函数的位置，将它们`call`的函数从原函数修改为mock函数。但是这样有一个很大的缺陷：有些调用并非直接通过call进行静态调用，而是使用函数指针或在PLT表中进行动态调用，无法从代码段中分析出这类函数究竟调用的是哪个函数。并且call可跳转的范围是有限的，跨动态链接库的情况下很可能无法直接跳转，需要使用长跳方式。

### 使用短跳指令hook函数

针对以上缺陷，可以换一种思路：虽然不是所有的call都使用同一种调用方式，但是所有的call最终都会前往函数的入口，那么就可以修改函数入口的代码，让其刚刚进入就跳转到mock函数。以`add`函数举例，我们尝试覆写其函数入口，将其跳转到`sub`函数。

```assembly
0000000000001169 <add>:
    1169:       f3 0f 1e fa             endbr64 ; 这个是安全机制，得留着
    ; 下面这些不要了，我们尝试直接覆写它
    116d:       55                      push   %rbp
    116e:       48 89 e5                mov    %rsp,%rbp
    ; 这段成为新的了，直接动手修改
    116d:       eb 12                   jmp    1181 ; 实际为 jmp $ + (0x1181 - 0x116d)
    116f:       89 e5                   ; 被损坏的指令,是原 116e 的 (48 [89 e5]) 部分
    1171:       89 7d fc                mov    %edi,-0x4(%rbp)
    1174:       89 75 f8                mov    %esi,-0x8(%rbp)
    1177:       8b 55 fc                mov    -0x4(%rbp),%edx
    117a:       8b 45 f8                mov    -0x8(%rbp),%eax
    117d:       01 d0                   add    %edx,%eax
    117f:       5d                      pop    %rbp
    1180:       c3                      ret

0000000000001181 <sub>:
    1181:       f3 0f 1e fa             endbr64
    1185:       55                      push   %rbp
    1186:       48 89 e5                mov    %rsp,%rbp
    1189:       89 7d fc                mov    %edi,-0x4(%rbp)
    118c:       89 75 f8                mov    %esi,-0x8(%rbp)
    118f:       8b 45 fc                mov    -0x4(%rbp),%eax
    1192:       2b 45 f8                sub    -0x8(%rbp),%eax
    1195:       5d                      pop    %rbp
    1196:       c3                      ret
```

这样我们就成功覆写了指令，让我们在C语言中实际尝试一下。如果读者想要使用这段代码，请保证您的代码是在Linux系统，X86-64（也即64bit）环境下编译执行。

```C
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

int add(int, int);
int sub(int, int);

int sub(int a, int b)
{
    return a - b;
}

int add(int a, int b)
{
    return a + b;
}

bool check_endbr64(void*ptr){
    uint8_t *p = (uint8_t*)ptr;
    // f3 0f 1e fa
    return p[0] == 0xf3 && p[1] == 0x0f && p[2] == 0x1e && p[3] == 0xfa;
}
void change_add()
{
    void *page_start = (void *)(uintptr_t)((uintptr_t)add & (~(uintptr_t)0xfff));
    if (mprotect(page_start, 4096, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
        perror("mprotect");
        exit(1);
    }
    uint8_t *func_start = (uint8_t*)add;
    // 跳过 0，1，2，3 字节，这里是保护指令，如果被修改
    // 在使用函数指针跳转时会发生错误
    intptr_t skip_endbr64 = 0;
    if (check_endbr64(func_start)) {
        skip_endbr64 = 4;
    }
    intptr_t offset = (intptr_t)sub - (intptr_t)(add + skip_endbr64 /*跳过endbr64*/);
    offset -= 5; // 减去 jmp 指令的长度
    if (offset > INT32_MAX || offset < INT32_MIN) {
        fprintf(stderr, "offset is too large\n");
        exit(1);
    }
    func_start[4] = 0xe9; // jmp 较长跳指令，占用5个字节
    *(int32_t*)(func_start + 5) = offset;
}

int main()
{
    int a = 4;
    int b = 5;
    int res;
    change_add();
    if (a == 4) {
        res = add(a, b);
        goto end;
    } else {
        res = sub(a, b);
    }
    printf("Oops, I can't be executed!\n");
end:
    printf("sum = %d\n", res);
    return 0;
}
```

在这次修改后，a仍然是4，b仍然是5，但是打印结果却并非是`sum = 9`，也并没有打印`Oops, I can't be executed!`，这说明分支仍然是 `a == 4`的分支，但是调用的函数变了。

```bash
qxy@qxy:~/testc$ ./a.out
sum = -1
```

显然，在修改过后，对add的调用就转为了对sub的调用，这是因为原本add的执行流程被我们修改为跳转到sub函数了。

有些读者可能会问：假如修改了动态链接库的函数，那么是不是会对其他使用这个动态链接库的函数造成破坏呢？

这个问题很有意思，考虑到了动态链接库在物理内存中是由多个进程共享的。 不过答案是不会的，我们可以查看每个进程对动态链接库的map来找到原因。

```
... 省略了其他map
7ff4590b2000-7ff4590da000 r--p 00000000 fd:00 393289                     /usr/lib/x86_64-linux-gnu/libc.so.6
7ff4590da000-7ff45926f000 r-xp 00028000 fd:00 393289                     /usr/lib/x86_64-linux-gnu/libc.so.6
7ff45926f000-7ff4592c7000 r--p 001bd000 fd:00 393289                     /usr/lib/x86_64-linux-gnu/libc.so.6
7ff4592c7000-7ff4592c8000 ---p 00215000 fd:00 393289                     /usr/lib/x86_64-linux-gnu/libc.so.6
7ff4592c8000-7ff4592cc000 r--p 00215000 fd:00 393289                     /usr/lib/x86_64-linux-gnu/libc.so.6
7ff4592cc000-7ff4592ce000 rw-p 00219000 fd:00 393289                     /usr/lib/x86_64-linux-gnu/libc.so.6
... 省略了其他map
```

以上面的`libc.so.6`为例，可以看到，无论是只读段、读写段还是代码段，其共享位都是p，这说明<del>libc是个勇士</del>其会被以私有内存的形式map，而操作系统对这种map会采取COW（写时复制）的机制，当进程对共享段进行修改时，操作系统会复制出一份单独的页面给该程序使用，避免进程对自身的共享库的修改影响到其他进程。

当然，这样做仍然有缺陷。

1. `jmp`指令的立即数寻址范围有限，只能寻址附近2GB的内存，想要更远就需要其他指令或跳板。
2. 对代码进行了修改，但是没有保存源代码，如果想解除mock就只能再从磁盘上读取文件。

### 如何跳得更远

jmp指令可以跳转前后2GB的空间，看上去很大了，很少有程序能有2GB的大小， 那么为什么还需要跳得更远呢？因为X64系统下，虚拟内存空间有非常大的128TB，并且通常so和主程序之间的空洞很大，远超2GB大小，这种情况下就需要长跳了。

长跳通常需要借助寄存器，那这样就需要增加指令的数量了。大致的流程如下。

1. 将地址加载到寄存器。
2. 跳转到寄存器所指向的位置。

在X86-64机器上，我们可以使用movabs指令后跟8字节立即数的方式指定指针的位置。

下面的例子介绍了应该如何修改 `libc.so.6`中的`rand()`函数。

```C
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>

int rand_only_1()
{
	return 1;
}
bool check_endbr64(void *ptr)
{
	uint8_t *p = (uint8_t *)ptr;
	// f3 0f 1e fa
	return p[0] == 0xf3 && p[1] == 0x0f && p[2] == 0x1e && p[3] == 0xfa;
}
void change_rand()
{
	void *page_start = (void *)(uintptr_t)((uintptr_t)rand & (~(uintptr_t)0xfff));
	if (mprotect(page_start, 4096, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
		perror("mprotect");
		exit(1);
	}
	uint8_t *func_start = (uint8_t *)rand;
	intptr_t skip_endbr64 = 0;
	if (check_endbr64(func_start)) {
		skip_endbr64 = 4;
	}
	intptr_t offset = (intptr_t)rand_only_1 - (intptr_t)(rand + skip_endbr64 /*跳过endbr64*/);
	offset -= 5; // 减去 jmp 指令的长度
	if (offset > INT32_MAX || offset < INT32_MIN) {
		// 这里直接的jmp指令就无法处理了，但我们可以将位置加载到寄存器中，然后跳转
		func_start[0] = 0x48; // movabsq $0x0, %rdi
		func_start[1] = 0xbf;
		*(int64_t *)(func_start + 2) = (int64_t)rand_only_1;
		func_start[10] = 0xff; // jmp *%rdi
		func_start[11] = 0xe7;
		return;
	}
	func_start[4] = 0xe9; // jmp 较长跳指令，占用5个字节
	*(int32_t *)(func_start + 5) = offset;
	return;
}

int main()
{
	change_rand();
	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < 10; j++) {
			printf("%d ", rand());
		}
		printf("\n");
	}
}
/*
qxy@qxy:~/testc$ gcc test.c -g && ./a.out
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1
*/

```

这里我们还观察到，这次的修改就需要函数至少有11字节的大小才能工作，如果函数过小，就有可能导致覆盖下一个函数的函数头，引起未知问题。然而，我们无法直接从代码段中获知函数的长度，这需要解析二进制文件才能得知。并且，即使是解析了二进制文件，仍然有可能有内部链接的静态函数因为符号表被剥离，程序无法解读，进而遭到破坏。

技术上，x86架构中的一个函数最少只会有1字节（也即只有1个ret指令），不过实际使用中，编译器往往会将一个函数填充到至少16字节，这给了我们充分的操作空间。如果函数长度大于5字节，也可以使用短跳后+寄存器跳的方式规避。但如果函数的大小真的只有1字节，那确实就无能为力了，不过只有一字节就说明这个函数单纯返回了，什么也没干，没有hook的必要，否则即使函数有2字节，都可以使用超短跳（仅2字节的jmp指令）跳过后再用跳板跳到寄存器处。

### 如何处理寄存器

有些读者可能会发现，上面的汇编代码破坏了寄存器`%rdi`，有可能导致未预期的行为，但这通常是可以接受的，因为`%rdi`在大部分ABI下，都是由调用方进行保存的，如果函数已经跳转，说明调用方已经妥善保存或不需要该寄存器的值，我们可以随意使用。

但是如果真的就有那么一种ABI，`%rdi`变成了参数寄存器，所有的寄存器和栈都需要完整保留给被调用函数呢？我们仍然有办法。

在寄存器跳转前增加一个`push %rdi`指令，将`%rdi`存入栈中，并且不直接跳转到mock函数，而是跳转到mock函数附近的一个中转函数（并非程序中的函数，更像是一段纯机器代码），在中转函数中使用`pop %rdi`指令将`%rsp`和`%rdi`恢复，并紧接着使用`jmp`指令直接跳转到对应位置。此时可以保证所有的寄存器以及栈都能完整传递给mock函数，就好像从来没有发生过跳转一样。



### 虚指针、函数重载等

只要了解了C++虚表的实现原理，这些东西都是很容易做到的，这个文章写了好几个小时了，上面C的看懂了，C++虚函数怎么mock的问题就只剩下怎么获取虚表了，这里我就不再赘述了，这不是本篇文章的重点。

# 总结

各种MOCK需要对内存和CPU都有较深的了解才能正确工作，并且这种方式需要指定CPU架构，我这里只写了x86-64架构的，arm64的又是不同的一种方式，如果长跳需要`ldr mov br`三个指令12字节和8字节的立即数20字节才能完成，不过由于arm64指令集是等长的，短跳情况下arm只会占用一个指令，这减少了不少短跳的负担。

其实还是C/C++积重难返，其他语言大多都是全量源码编译，mock这种事情编译期就能搞定。而Python、Java这样的字节码语言就更简单了。

有的时候写C真的会有一种自己是很牛逼的大佬的错觉。