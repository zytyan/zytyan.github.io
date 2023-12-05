---
title: Redis与fork系统调用
date: 2023-09-06 23:39:44
tags: [Redis, Linux, 多进程]
---
# Redis为什么不能在Windows上工作

因为Redis虽然使用ANSI C编写，兼容所有编译器版本，但是其调用了只有*nix系统才存在的fork系统调用，而Windows上没有这个调用。



# 为什么Redis需要fork

Redis作为非常快速的内存数据库，也需要持久化保存数据到硬盘的能力。如果单纯凭借自己实现，就会产生很多竞争或者数据一致性、或者是保存时不得不暂停写入（甚至读取）的问题。而使用fork系统调用，就可以充分利用fork的COW（写时复制）技术来加速。