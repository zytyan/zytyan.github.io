---
title: Python排序详解
date: 2024-02-17 16:22:15
tags: [python, 排序, 算法]
---

# Python中的排序方法

原生Python中有两种常见的排序方法，分别是 `list.sort()`与`sorted()`内置函数。

其中，`list.sort()`是列表的排序方法，是一个原地排序方法，使用尽可能少的额外空间，它只能在列表上使用。

而`sorted()`则会返回一个已经排好序的列表，其参数可以是任何可迭代对象，无论是列表、元组、字符串、range、字典、甚至使用函数生成的迭代器，只要其是一个可迭代对象，并且返回的对象可以比较，那就可以进行排序。其行为可以用以下代码模拟。

```python
def sorted(iterable):
    tmp = list(iterable)
    tmp.sort()
    return tmp
```

所以当传入字典参数时，其返回的结果不会包括字典的value，而仅会包括字典的key。

# 排序的基本规则

## 排序算法

Python中的排序算法为TimSort，这种排序算法是一种稳定的排序算法，同时其保证最坏时间复杂度为$O(n log(n))$。而在数组已经有序时，其时间复杂度能低至$O(n)$​，即每个元素仅比较一次。

稳定意味着对于在比较中相等的值，在排序后依然保持原来的顺序不变。

## 排序会优先使用小于进行

Python在排序时，两个对象的比较会优先使用`<`进行比较，而如果`<`没有定义，则会尝试使用`>`进行比较，如果这两者都没有定义，那么就会抛出一个`TypeError`，表示不能对此进行排序。

# 排序的两个关键字参数

无论是`list.sort()`还是`sorted()`，其都接受两个关键字参数`key`和`reverse`。



## `key`参数

key参数可以决定Python如何获取可比较的对象。当传入此参数时，Python比较时不会简单的进行 `a<b`这种方式，而是会进行`key(a) < key(b)`的比较，这可以让我们操作比较的细节。

例如，传入`abs`可以让Python按绝对值进行排序。

```python
sorted([-1,-5,3,4])          # [-5, -1, 3, 4]
sorted([-1,-5,3,4], key=abs) # [-1, 3, 4, -5]
```

也可以通过匿名函数，让排序只对元组的某一个位置进行排序，而不是从头开始排序。

```python
lst = [("Alice", 27), ("Bob", 26), ("Altman", 26)]
sorted(lst)                     #[('Alice', 27), ('Altman', 26), ('Bob', 26)]
sorted(lst, key=lambda x: x[1]) # [('Bob', 26), ('Altman', 26), ('Alice', 27)]
```

也可以巧妙的通过加符号而让一个升序，一个降序。

```python
lst = [(1, 3), (5, 6), (1, 4)]
sorted(lst, key=lambda x: (x[0], -x[1]))  # [(1, 4), (1, 3), (5, 6)]
```

来自一个B站用户的想法：想让元组中第一个数字升序，第二个字符串降序。由于字符串不能使用负号反转比较结果，我们可以反转整数的比较结果，让第一个数字降序，第二个字符串升序，并且指示反转排序结果，可以看到这种取巧的方式也依然可行且为稳定排序。

```python
lst = [(1, 'a', 'first'), (1, 'b'), (1, 'a', 'second'), (2, 'a')]
sorted(lst, key=lambda x: (-x[0], x[1]), reverse=True)
# [(1, 'b'), (1, 'a', 'first'), (1, 'a', 'second'), (2, 'a')]
```

### 其他使用方式

类似的，可以使用匿名函数对字典的某个键或者对象的某个属性进行排序，以让不可比较对象能够通过某种方式进行比较。

对于列表中被排序的每个元素，即使多次被比较，key函数也只会被调用一次。而显然，为了排序，key函数也至少会被调用一次。

Python标准库`functools`中有一个`cmp_to_key`函数，可以让大部分情况下自定义比较方法更简单，我们后文会提到。

### 注意事项

对于`list.sort()`而言，CPython中有一个实现细节：在排序过程中，会将列表的长度设为0，并将内容指针设为空。同时，在排序结束时，会检查列表是否被**修改过**。

如果排序时通过key函数访问原始的列表，打印该列表可以发现该列表的内容为空。如果此时在key函数中修改了列表，则可能会出现`ValueError: list modified during sort`，即使修改进行了`list.clear()`操作仍然会导致该问题。除非并未实际修改（如`my_list.extend([])`）,该行为可能会因为版本不同而不同，我们需要知道的只是如果排序中需要访问列表，我们需要访问列表的复制。

当然，不进行原地排序的`sorted()`不会受到该特性的影响。

## `reverse`参数

`reverse`参数比较简单，就是反向排序，默认的排序方案是小的排在前面，通过指定`reverse=True`，可以让大的排在前面。和先进行排序，再进行反转不同的是，这种排序方式仍然是**稳定**的。

# 富比较

## `__cmp__`与富比较方法

使用C语言的朋友们一定知道，在C中的各类比较并非通过任何运算符重载，而通常是通过一个cmp函数进行比较。我们以`strcmp(char* str1, char* str2)` 为例，其可能返回0、-1或1，分别代表`str1 == str2`，`str1<str2`, `str1>str2`。这种比较方式的优点是仅需要实现一个比较函数，就可以完成排序、寻找最大值等多种需要比较的场合，C++也于C++20引入了`<=>`（三向比较运算符）支持这种方式。

事实上，在Python2时代，Python也使用这种方式来重载所有比较运算符，然而这有一个很难注意到的问题：这里的比较结果只能为0、大于0或小于0，也就是说一个值和另一个值比较，不是大于或小于，就是等于，但是也有一些时候我们的数据不是全序的，会需要既不大于，也不小于，也不等于的比较，此时`__cmp__`方法就无能为力了。

上述情况中最常见的属于浮点数NaN，我们来看代码。

```python
nan = float('nan')
print("nan == nan", nan == nan)  # False
print("nan > nan", nan > nan)  # False
print("nan < nan", nan < nan)  # False
print("nan >= nan", nan >= nan)  # False
print("nan <= nan", nan <= nan)  # False
print("nan != nan", nan != nan)  # True
```

类似的，还有SQL中的NULL比较等，也都不是cmp函数可以覆盖的范围。

所以在Python3中，Python不再使用`__cmp__`运算符，而是将其分为了6个富比较运算符。

-  `__eq__` 重载`==`运算符，`a == b`相当于`a.__eq__(b)`
- `__ne__` 重载`!=`运算符，`a != b`相当于`a.__ne__(b)`
- `__lt__` 重载`<`运算符，`a < b`相当于`a.__lt__(b)`
- `__gt__` 重载`>`运算符，`a > b`相当于`a.__gt__(b)`
- `__le__` 重载`<=`运算符，`a <= b`相当于`a.__le__(b)`
- `__ge__` 重载`>=`运算符，`a >= b`相当于`a.__ge__(b)`

所以在Python中，我们也不是不可能可能见到`a == b and a != b`为真的情况，不过应该没人会写这种代码。我们也可能见到 `a==b`的返回值并非bool值的情况，而这种情况常常发生在numpy中。



## 标准库中的`cmp_to_key`

默认情况下，这个函数由C语言实现以提高速度，不过标准库中仍然提供了纯Python实现的fallback，该实现和C语言实现具有同样的行为，并且这个实现很简单，我们直接看实现源码。

（是的，C语言实现中也使用了对象和0的Rich Compare，而并非仅接受数字的比较，所以你可以在cmp函数中返回任何可以和0相比较的对象。）

```python
def cmp_to_key(mycmp):
    """Convert a cmp= function into a key= function"""
    class K(object):
        __slots__ = ['obj']
        def __init__(self, obj):
            self.obj = obj
        def __lt__(self, other):
            return mycmp(self.obj, other.obj) < 0
        def __gt__(self, other):
            return mycmp(self.obj, other.obj) > 0
        def __eq__(self, other):
            return mycmp(self.obj, other.obj) == 0
        def __le__(self, other):
            return mycmp(self.obj, other.obj) <= 0
        def __ge__(self, other):
            return mycmp(self.obj, other.obj) >= 0
        __hash__ = None
    return K
```

不知道该说啥，着实是简单了点，让我们看一个实际案例：让字符串升序排在前面，让数字降序排在后面。

```python
from functools import cmp_to_key


def cmp(a, b):  # 字符串升序排前面，数字降序排后面
    # True 和 False几乎在任何情况下均可以当作 1 和 0 看待
    if isinstance(a, str) and isinstance(b, str):
        return (a > b) - (a < b) # True - False = 1, False - True = -1
    elif isinstance(a, int) and isinstance(b, int):
        return (a < b) - (a > b)
    else:  # 类型不同时，字符串要排前面就需要比其他类型小，所以返回-1
        if isinstance(a, str):
            return -1
        else:
            return 1


def main():
    lst = [3, 2, 1, 'a', 'b', 'c']
    lst.sort(key=cmp_to_key(cmp))
    print(lst)


if __name__ == '__main__':
    main()

```

