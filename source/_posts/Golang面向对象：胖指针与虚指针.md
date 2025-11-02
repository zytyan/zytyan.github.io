---
title: Golang面向对象：胖指针与虚指针
date: 2025-11-01 18:35:01
tags:[Golang, 面向对象]
---

传统的面向对象设计中，继承机制带来了很多不必要的复杂性。继承层级复杂后，往往伴随着代码耦合性高、难以修改等情况。新兴语言中，比较保守的如Kotlin、仓颉等不再默认允许类被继承，而是需要通过`open`关键字显示声明；比较激进的语言如go、rust则彻底抛弃了继承，转而使用组合+接口的形式。

然而，纯粹的组合+接口像是回到了C语言时代：类型无法简单复用已有的代码，需要写很多样板代码才能将接口的实现委托给内部的某一成员，这也很让人不爽。为了避免大量样板代码，Go和Rust都做出了自己的改进。

## Rust的trait

trait是从函数式语言中借鉴来的机制。它类似于面向对象语言中的接口，但是要比接口更强大。它可以为其他包中的类型定义trait，也可以通过其他trait自动为所有类型实现另一个trait。

## Go中的匿名结构体

Go则使用传统的接口方式，但是增加了匿名结构体的概念。Go的匿名结构体非常像是继承，它可以为新结构体自动实现匿名结构体中已实现的接口，以减少无意义的样板代码。

```go
type MyFile struct {
    fs.File
}
func (m *MyFile) Read(buf []byte) (int, error) {
    fmt.Printf("Read buf size: %d\n", len(buf))
    return m.Read(buf)
}
// 但是也能自动实现 Close、Stat等方法
```

## 抛弃继承的实现方法

由于不再需要继承，自然也就没有了“父类指针（引用）指向子类对象”的需求。此时，一个变量的类型要么是接口，要么是具体类型，这类语言在实现上也做了修改，它们不再使用传统面向对象中将运行时多态的虚指针放在对象头中的方案，转而使用了只在需要接口多态时才将指针与多态指针打包作为变量的技术。

```C
struct Inherit{
    // 传统面向对象中，一个对象若有可被继承的方法，
    // 则必然有一个虚指针，无论其是否真的被使用。
	void *vptr;
	int data1;
	int data2;
};

struct Comb {
    // 然而在组合编程中，无论是否有实现接口，
    // 都无需虚指针，只在声明了接口类型的变量时，
    // 会将接口相关的函数一并打包为一个胖指针。
    int data1;
    int data2;
};

struct CombInterface{
    // 所谓胖指针，就是一个指针指向真正的数据，
    // 另一个指针指向方法字典（类似于虚表）。
    // 当使用接口或trait类型时，编译器会自动将类型转换为胖指针。
    struct Comb*real_data;
    void *dict;
}
```



### Go组合逻辑中缺少的能力

然而使用胖指针实现，就缺少了一项重要能力：将实现委托给父类的能力。让我们看以下一段简单的C++代码。

```C++
#include <iostream>
class Shape {
   public:
    virtual double area() = 0;
    virtual void printArea() { std::cout << "area = " << area() << "\n"; }
};

class Rect : public Shape {
    double w;
    double h;

   public:
    Rect(double w, double h) : w(w), h(h) {}
    virtual double area() override { return w * h; }
};

int main() {
    Rect rect = Rect(3.0, 4.0);
    Shape& shape = &rect;
    shape.printArea();  // area = 12
}
```

在这段代码里，程序只需要实现`area`方法，就可以自动父类中获得`printArea`方法。Java也有类似的默认接口实现。Rust中也可以为`trait`中的方法添加默认实现，然而在Go中，却缺少了这项能力。

在Go中，一个典型的方法声明是这样的。

```go
func (t *Type) Method(){}
```

由于非接口类型不存在多态，且Go的方法接收者不允许为接口类型，所以当在Go方法中调用其他方法时，所有的方法都是静态分发的，实现不了C++中的效果。

```go
package main

import "fmt"
type IArea interface {
    Area() float64
    PrintArea()
}
type Shape struct{}

func (s *Shape) Area() float64 { panic("not impl") }
func (s *Shape) PrintArea() {
    	// 由于类型是Shape，即使是通过Rect调用PrintArea，也无法正确调用。
        fmt.Printf("area = %f\n", s.Area())
}

type Rect struct {
        Shape
        w float64
        h float64
}

func (r *Rect) Area() float64 {
        return r.w * r.h
}
func main() {
        r := Rect{w: 3.0, h: 4.0}
        r.PrintArea() // panic!
}
```

不过这其实并不是多么重要的问题，这种需求现在并不是太多，所以也不是不能接受，而且Golang完全可以使用匿名函数实现类似的能力，这里也就更显得没那么重要了。
