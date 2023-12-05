---
title: Python哈希冲突浅析
date: 2022-06-11 18:27:20
tags: [Python, hashmap, 数据结构]

---

Python字典是一种常用的数据结构，内部采用hashmap数据结构，利用元素的hash值作为索引。

# 字典实现

首先实现源码在 [Github:Objects/dictobject.c](https://github.com/python/cpython/blob/main/Objects/dictobject.c)

与Java的HashMap不同，Python字典解决冲突的机制并非拉链法，而是开放定址法。插入元素index冲突时会采用一种特殊的算法来计算新的索引，直到找到空位为止。

当字典已使用空间达到2/3时，继续插入会触发扩容，通常初始字典的大小为8，扩容大小为原来的2倍大，即字典的slot大小只会是2的幂数。然而字典中也会有其他元素，所以实际占用空间并非2的幂数。



# 哈希冲突

[oCERT-2011-003](http://ocert.org/advisories/ocert-2011-003.html)提出了一种哈希冲突的问题，即故意制造大量冲突对服务器造成DOS攻击。该漏洞广泛影响了多种编程语言，包括当时的Java、Python、Ruby、PHP，不过C、C++由于当时没有标准库内置hashmap幸免。

漏洞的根源是由于这些语言使用非加密hash函数，导致攻击者很容易执行原相攻击和第二原相攻击。通常语言内置hash函数的结果只会被作为hashmap、hashset等数据结构的索引使用，并不用来执行与机密数据相关的操作，所以语言采用非加密hash无可厚非。然而攻击者可以利用这个漏洞恶意制造大量相同hash值的不同对象，迫使hashmap退化为线性结构而引发DoS攻击，严重的可引起服务器宕机。

如今，许多语言都对这种方法做了防范，例如Python在对str和bytes进行hash时会采用一个随机值“加盐”，这个随机值被称为hash seed，hash seed在同一进程中保持一致，但在独立启动的不同进程中是随机的。这24字节的随机值可以保证网络上的攻击者无法发动上述攻击。而如果遇到可能由于hash函数导致的bug，也可以通过设置`PYTHONHASHSEED`环境变量的值来确保每一次hash结果相同来复现bug。

其它语言中，Java会在hash冲突过多时将冲突的部分转化为红黑树来保证速度不会过慢。Rust直接采用了加密hash，但也导致了在需要大量计算hash时速度有明显劣势，需要调整编译flag以采用常规hash。

在Python官方文档中，其表明精心构造的输入可能会导致字典插入时具有O(n^2)级别的时间复杂度，然而我查阅了字典实现的源码后发现，其可能拖慢插入速度的总共有两个函数，分别是`find_empty_slot:1165`和`dictresize:1405`，前者寻找空槽明显可以看出是线性复杂度，而后者则是字典扩容，虽然需要重新获取key对象的hash，但是可以看到也是线性的复杂度。同时前文提到扩容为倍增扩容，大量输入时一段时间内的平均触发次数只会越来越少。从理论上讲是不会有平方复杂度的。

但实际情况可能和理论有所差别，所以我写了一段测试代码用来测试插入时间。测试的机器CPU为 i7-7700HQ ，内存为 DDR4 2133MHz ，下面是测试代码。

```python
import time


class TestHashConflict:
    def __hash__(self):  # 模拟恶意构建的冲突hash，永远返回100作为hash值
        return 100


class TestNiceHash:
    def __hash__(self):  # hash自己的id（类似hash指针），是一种简单的hash方法
        return hash(id(self))


def add_objs(n, dic, factory):  # factory不同，使用的对象也不同
    start = time.perf_counter()  # 使用perf_counter 计算hash时间，避免time.time()不精确造成的问题
    last = start
    for i in range(n):
        o = factory()
        dic[o] = i
        if i % 1000 == 0:  # 每添加1000个计时一次
            now = time.perf_counter()
            print(f"{i}\t{now - start}\t{now - last}")
            last = now
    end = time.perf_counter()
    print(f"end time:{end - start}")


def main():
    n = 1_000_000
    add_objs(n, {}, TestNiceHash)  # 先测试添加一百万个普通的hash对象，总用时大约0.933秒
    add_objs(n, {}, TestHashConflict)  # 一百万个没添加完，添加到90万时总用时大约2.5万秒


if __name__ == '__main__':
    main()


```

这里我强行定义TestHash对象的hash值永远是100，而不同对象并不相等。同时为了说明计数和打印的代码几乎不会对性能造成影响，同样插入一百万hash自身id的对象模拟正常对象。在空字典中插入一百万正常对象时总用时只有约0.933秒，而插入一百万hash相同的对象时总用时高达数万秒，事实上我只插入了90万hash相同的对象，总用时已经来到了2.5万秒左右。将数据导入execl表中，可以看到平均插入的时间确实是预期的O(n)。

![image-20220611200444050](/images/image_280.png)

