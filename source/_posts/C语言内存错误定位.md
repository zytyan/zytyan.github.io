---
title: C/C++常见内存问题定位
date: 2023-09-03 21:42:55
tags: [C, C++, 内存]
---

本文旨在记录Linux下C/C++常见的内存错误。

## SIGSEGV

段错误，最常见的内存错误，是由于使用错误的方式访问内存导致的，常见的错误方式有下面几种。

1. 解引用空指针，这是最可能发生的段错误，在有core dump文件时也很容易定位，看到fault addr时0时即可基本确认该问题是引起的。

   然而需要注意的是，解引用空指针并非绝对不可行的，之所以空指针指向空而不是虚拟内存的起始位置，是因为大部分情况下系统并未提供虚拟地址0附近内存区间的映射，如果一定要映射，在有root权限的情况下是可以通过`mmap`实现对空指针的映射的。

   ```C
   int main() {
       int *p = NULL;
       *p = 23456; // segmetation fault
   }
   ```

   

2. 解引用值为未被映射的地址的的指针。通常是未初始化的指针变量造成的野指针导致的。

3. 尝试修改不具有写权限的内存。

   ```C
   int main() {
   	char *str = "This is a static string";
   	str[0] = 't'; // segmetation fault
   }
   ```

4. 未开启栈保护时，发生栈溢出的情况。此时函数return时会由于栈中存储的caller地址错误而大概率产生segment fault.



## SIGABRT

程序终止，这个信号通常是由程序自身发出，编程人员也可以通过手动调用`abort()`函数来为本进程产生这个信号，这个信号结束的程序通常是在运行时检测到的错误。常见的错误原因有下面几种。

1. double free，即free已经free过的指针，glibc检测到堆内存中的double free会直接调用`abort()`终止进程。

   ```C
   #include<malloc.h>
   
   int main() {
       void *ptr = malloc(100);
       free(ptr);
       free(ptr); // Aborted
   }
   ```

   

2. stack smashing，栈损坏，当开启了gcc的栈保护功能时，gcc在每次函数退出时都会尝试检查栈中的数据是否被修改，如果被修改则会终止程序，避免栈溢出造成更大的问题。

   ```C
   #include <stdio.h>
   #include <string.h>
   #include <assert.h>
   
   void print_hex(void* data, size_t len) {
       char* buf = (char*)data;
       for (int i = 0; i < len; i++) {
           printf("%02X ", (unsigned char)buf[i]);
           if (i % 16 == 15) {
               printf("\n");
           }
       }
   }
   
   int read() {
       char buf[10] = {0};
       printf("buf = %p\n", buf);
       print_hex(buf, 80);
       printf("\n");
       strncpy(buf, "0123456789A", 11);
       print_hex(buf, 40);
       // aborted，因为libc检测到了栈损坏。
       return 0;
   }
   
   int main() {
       int res = read();
       printf("res = %d\n", res);  // 不会被打印，已经aborted了
   }
   
   ```

   

3. 堆内存结构损坏。当malloc使用的元数据被修改后，其检查到无法进行正常内存管理时会释放内存。

4. `assert`失败，断言失败也会产生该问题。

   ```c
   int main() {
   	asset(0 == 1);// false, aborted	
   }
   ```

   

## SIGBUS

在x86 CPU上这种信号并不常见，但可能会在ARM CPU上出现。

1. 非对齐的原子内存访问，例如`ldaxr`，`stlxr`两个指令，在未对齐（寄存器为x时为8字节对齐，w为4字节对齐）的情况下会产生该信号，其和系统是否允许对齐无关。

   ```assembly
   :main
   	ldaxr x0, =0x12345677 # 这里会产生sigbus，因为有非对齐的原子内存访问
   	ldaxr x0, =0x12345678 # 这里会产生段错误，因为对齐了，但是虚拟内存里没有这个页，当然如果事先做了map就没问题。
   	
   ```

   

2. 非对其的指令访问。ARM指令集为等长的，均为4字节，当使用b类指令跳转到一个没有四字节对齐的地址也会产生该信号，通常这在C面向对象编程时错误的修改函数指针有关。不过在X86上可能会表现为Illegal Instruction，因为X86架构并非等长指令，即使不对齐也可以执行指令，但大概率这个指令是没见过的指令。

   ```c
   #include<stdio.h>
   #include<stdint.h>
   
   void function() {
       printf("做点什么");
       return;
   }
   
   uint64_t counter[8] = {0};
   void(*func_ptr)() = NULL;
   
   int init() {
       // 不相关代码...
       func_ptr = function;
       // 不相关代码...
       return 0;
   }
   
   int somthing_counter_add() {
       // 这里其实是对counter中某个元素的自增操作，但是由于编译器内存布局具有一定
       func_ptr++;
   }
   
   int main() {
       init();
       somthing_counter_add();
       func_ptr();// sigbus
   }
   ```
   
   



