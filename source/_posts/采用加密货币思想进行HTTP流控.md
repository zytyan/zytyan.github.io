---
title: 采用加密货币思想进行HTTP流控
date: 2022-05-11 21:15:30
tags: [HTTP, 加密货币]
---
流控是常见的功能，很多重要位置都有流控。

# 为什么进行流控

在实际生产中，我们的很多资源都是有限的，比如视频转码的CPU以及GPU资源、流量配额、邮件配额、数据库连接等，这些资源如果不提前预计分配，导致某一配额用光或队列爆满，就会导致服务暂时甚至长时间的处于不可用状态，会严重增加维护成本。

当然，如果我们面对的都是普通用户，那么在正确估计了用户量的前提下，无论用户是网页查看还是API调用，都是很难发生资源耗尽这种事情的。但如果有人对我们进行CC攻击，情况就不一样了，恶意攻击往往会对常见的资源最紧缺的地方发起，让我们必须使用一些手段来保证资源不被迅速耗干。在仅能由客户端或网页访问的地方，我们可以部署不同的人机验证程序，但在公开的API中，我们就需要流控手段来限制访问频率。

## 常见的流控手段

常见的流控手段有计数器算法、漏桶算法、令牌桶算法等，不过在这里我只是提一下名字，这些传统算法并不在这篇文章的讨论范围内。

# 加密货币思想

加密货币在前几年风靡互联网，其核心的思想就是**工作量证明（POW）**，通过计算和验算所需的算力不对等来保证加密货币的货币属性。而服务器也可以通过加密货币的核心思想要求调用接口客户端证明一定工作量，而证明工作量则需要一定的时间，通过这种方式达到流控的效果。

举一个简单的例子。

首先服务器随机生成一个token并保存，将token传给客户端后，让客户端随机生成nonce，满足 `sha256(token+nonce) < 0x000...fff`。由于加密散列的抗原相特性，客户端唯一的方法就是重复生成nonce直到满足要求，然后将nonce和token传回服务器，而服务器仅需验证一次即可。

当然，服务端也可以采用`AES(timestamp+token)`的方式进一步减少储存token的内存要求，选用更快的加密散列函数（如blake2b）而设定更小的范围来进一步减小对CPU的要求。

## 特点以及为什么没人用

通过POW，确实是一种与硬件绑定的、难以绕过的验证方式，这种方式也平衡了只有少量客户端的家用IP与有大量客户端的公司、学校IP，使得不同IP下的用户有着较为一致的体验。在面对攻击时，如果攻击者没有足够的算力也难以破解。

然而它的缺点也同样非常明显，首先是算力，即使可以用wasm最大化CPU算力，同样面临着不同机器算力不同的问题，手机显然要比高配电脑算的更慢，然而当前的环境下，手机往往是访问的主要流量来源，这就让这种方法的实用性进一步下降。而对于防范被攻击来说，基于IP的令牌桶算法已经具有较好的效果了，对人类使用的接口，还可以增加reCAPTCHA这样的人机验证装置，这种方式的实际适用范围很小。

## 概念验证代码

这篇文章算是群友的突发奇想，我在此附上一个概念验证代码，由python编写。

```python
import hashlib


def inc(b: bytearray):
    i = 0
    while i < len(b):
        # 这里采用的是大端序自增，实际上无论怎么自增影响都不大
        tmp = b[i] + 1
        b[i] = tmp % 256
        if tmp % 256 == 0:
            i += 1
        else:
            break


def client_calc(token: bytes, target: bytes):
    nonce = bytearray(b"\0" * 16)
    while True:
        res = hashlib.sha256(token + nonce).digest()
        # 客户端不断计算计算新的值
        if res < target:
            print("[Client]\tFound: {}, nonce: {}".format(res.hex(), nonce.hex()))
			# 满足条件返回
            return nonce
        inc(nonce)


def fake_client_calc(token: bytes, target: bytes):
    print("[Client]\tFake calc")
    # 如果不计算而返回一个假的就会变成这样
    return b"fake nonce"


def server_run(calc_func):
    print("[Server]\tcalc_func: {}".format(calc_func.__name__))
    token = b"token token"  # 此处可以随机生成，这里仅作演示

    # 要小于这个才能认为通过，如果运行慢可以适当增大
    target = bytes.fromhex("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")

    nonce = calc_func(token, target)  # 这里模拟将token发送给客户端并让其计算
    digest = hashlib.sha256(token + nonce).digest()
    if digest < target:
        print("[Server]\tFound: {}, nonce: {}".format(digest.hex(), nonce.hex()))
    else:
        print("[Server]\tInvalid nonce: {}".format(nonce.hex()))


def main():
    server_run(client_calc)
    print()
    server_run(fake_client_calc)


if __name__ == '__main__':
    main()

```



