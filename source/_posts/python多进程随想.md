---
title: python多进程随想
date: 2022-04-06 04:36:37
tags: [python, 编程, 多进程]
---
为什么Python多进程入口需要用`if __name__ == '__main__'`包裹呢？
实际上，如果在Linux上运行，通常情况下即使没有这行判断也并不会造成问题。
因为在类Unix系统中，程序可以通过fork创建自身的副本，这也是Python在Linux系统下产生子进程的默认行为，此时作为副本的子进程和父进程的内存是完全一致的，程序只需要通过fork的返回值判断是父进程还是子进程后，子进程运行指定函数，父进程继续向前运行。
然而在Windows中，程序只能通过 `spawn` 方法来执行一个全新的子进程，内存不共享，Python只能重新加载所有脚本，通过 `pickle` 给子进程传递相应的被序列化的函数对象执行，如果不做限制，就可能导致无限递归地产生子程序。不过Python3做了防备，不会出现这种无限递归的问题，会直接报错。而Python2则会无限递归耗尽系统资源。
类似的原因，你不能在Windows中将闭包函数作为多进程的run target参数，因为闭包在Python中不能被序列化，然而Linux通过fork调用，子进程获取了父进程的内存，所以是可以使用闭包作为target参数的。
同样的，一些可变的全局变量在不同系统中表现也不同，也许会有意想不到的错误。
有空附上源码分析和

最后附上一个原型验证代码。
```python
import multiprocessing
import os
import time


def test_a():
    print("print in test_a name:{} pid:{}".format(__name__,os.getpid()))
    if __name__ != '__main__':
        print("sleep 1s"  )
        time.sleep(1)
        print("sleep over"  )
def test_closure():
    def test_b():
        # 当然，python没有静态优化，即使这个闭包没有使用所在函数的任何属性，也依然不能被序列化
        print("print in test_b name:{} pid:{}".format(__name__,os.getpid()))
        if __name__ != '__main__':
            print("sleep 1s"  )
            time.sleep(1)
            print("sleep over"  )
    return test_b

def main():
    # __name__ 在主进程中为 '__main__', 在子进程中为 '__mp_main__'
    # 此处的注释无特殊说明都是描述的在Windows系统下的情况
    print("create process object{}".format(__name__))
    # 当target为 `test_a` 时，如入口没有 if __name__ == '__main__'保护，则子进程运行到下面的 `p.start()` 处报错。
    # p = multiprocessing.Process(target=test_a) 
    
    # 当target为 `test_closure()` 时，无论有无保护主进程都会在运行至`p.start()`处报错，因为闭包结构并不能被Python序列化，也就无法正确产生子进程。
    p = multiprocessing.Process(target=test_closure())
    
    print("start process{}".format(__name__))
    p.start()  # 主进程或子进程运行到此处时会报错
    print("join process{}".format(__name__))
    p.join()


if __name__ == '__main__': # 如果删去本行判断,产生的子进程运行到 `p.start()` 时会报错
    print("before main, __name__ is :{}".format(__name__))
    main()
    print("after main, __name__ is :{}".format(__name__))

```