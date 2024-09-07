---
title: 如何配置iptables
date: 2024-08-26 01:07:15
tags: [Linux, 网络, iptables]
---

`iptables` 是Linux中的网络工具，其工作在用户空间，可以用来操作内核空间的`netfilter`模块，以自由控制经过该主机的包。它替换了老版的`ipchains`，提供了更多的功能。而其后继者是`nftable`，它比`iptables`更灵活、更具伸缩性、性能也更好，提供了更多的功能。但是因为`iptables`还存在，功能在大多数情况下也够用，最重要的是教程多，各种AI给出的回答也更准确全面，所以现在还是推荐学`iptables`。

顾名思义，iptables主要作用在网络层，但事实上它也可以获取其他层的数据。管理员可以通过命令将这些数据做为判断条件，对数据包执行不同的操作。
# 处理单元
iptables对每个数据包进行处理，即使是TCP也不会认为其是一个流，但可以在规则中获取流相关的信息。对于超过MTU而必须分片的UDP数据包，其看到的是分片后的数据包，每个分片都会单独处理。

# 表和链
## 什么是表和链
在iptables中，有两个核心概念：表与链。定义上，表包含了用于处理包的**规则链**，每个表都对应了不同的包处理功能。而包会遍历规则链中的规则。

在了解`iptables`前，可以先简单了解它的前身：`ipchains`，`ipchains`并不存在表，只有三条预置链`input`, `output`和`forward`，所有的操作，包括过滤、NAT和TOS修改都在这些链上完成，并且转发流量也会经过`input`和`output`链。在设计上，这更符合直觉，但随着网络规模日益扩大，复杂度也越来越高，在同一个链上完成的操作就变得越来越多，规则也越来越容易让人迷惑。并且三条链也无法完成一些NAT任务。由于以上多种缺点，`iptables`引入了表，将不同方面的功能放入不同的表中，降低耦合，增强扩展性，并提高性能。

在用户实际理解中，链可以认为是数据的移动路径，在这个路径上移动时会遍历规则，当遇到第一个匹配的规则时会执行相应的动作。而表可以认为是对数据进行的操作。

通常的iptables教程中均会提到其 **“四表五链”** ，也就是`filter`, `nat`, `mangle`, `raw`四个表，和`INPUT`, `OUTPUT`, `FORWARD`, `PREROUTING`, `POSTROUTING`五个链。但事实上这只是当前Linux主机常见的模式，表和链都可以根据需要自行增删。修改表通常涉及到内核模块，所以比较少见。一种比较常见的增加表的情况是当设备启用了SELinux时，会增加一个`security`表，用于控制流的安全上下文。AppArmor对`netfilter`的支持没有SELinux完善。此外，还有类似`xt_conntrack`的模块可以为iptables增加新的功能。

与表不同，链可以在用户空间使用命令直接进行增删，通过增加链可以让数据流更为清晰，简化管理；也可以让减少数据在主链的匹配次数，提高运行速度。例如Docker就在iptables中增加了属于自己的`DOCKER-USER`链。

## 五条内置链
链是数据运行的链路，当前内核版本下，共有五条预置链，它们各自有不同的功能。在下面的内容中，如果没有特殊说明，我会假设本机作为一个路由器，管理一个内网网段`192.168.1.0/24`，拥有一个内网IP `192.168.1.1` 和一个公网IP `13.0.7.33`，并开启了一个HTTP服务器监听80端口。
### `INPUT`链
所有前往本机的数据将会经过该链，如果本机作为路由器转发流量，转发的流量不会经过该链。本地回环数据包也会经过该链。如果在`PREROUTING`阶段使用NAT方式让流量进入本机，那么流量也会经过该链。
例：`43.1.3.55:44291 -> 13.0.7.33:80`
`192.168.1.103:53211 -> 192.168.1.1:80`
`127.0.0.1:5555 -> 127.0.0.1:5556`
### `OUPUT`链
本机应用程序发出的数据包会经过该链，类似于`INPUT`链，本机转发的流量不会经过`OUTPUT`链。数据包经过`OUTPUT`链后会进行一次路由决策，用于决定该向哪里转发数据包。
例：`13.0.7.33:80 -> 43.1.3.55:44291`
`192.168.1.1:80 -> 192.168.1.103:53211`
`127.0.0.1:5556 -> 127.0.0.1:5555`

### `FORWARD`链
如果本机作为路由器转发流量，则本机需要转发的数据会经过FORWARD链。想要该链起作用需要启用流量转发功能。
例：`192.168.1.103:11032 -[SNAT]-> 8.8.8.8:53`

### `PREROUTING`链
任何外部流量，如果目标MAC地址为本机或广播，则会经过该链。通过该链后本机会进行路由决策，区分到达本机的包和转发包。
由本机发出的数据包如果在路由决策后直接回到本机，则不会经过该链，所以本地回环包以及目的地址为自身IP的包不会经过该链。
即使网卡工作在混杂模式下，MAC不相符的数据包也不会由内核协议栈处理，而是直接丢弃（或被流量监控工具捕获）。

### `POSTROUTING`链
所有本机出方向的包，包括转发包本地回环包，都会经过该链。该链为所有路由都已决定后，在离开本机前做最后一步处理的链。其最主要功能为源NAT，用于为本机和子网内设备同时提供NAT服务。
![During the lifecycle of "iptables", in which step, will kernel take  advantage of "route table"? - Unix & Linux Stack Exchange](/images/iptables_chains.png)

## 四个内置表
### `filter`表

`filter`表是最常用的表，用于数据包过滤。通常应用是配置Linux本机防火墙，避免外网随意访问对外暴露的端口。例如仅允许部分IP段访问SSH端口、仅允许反向代理出口或WAF出口访问HTTP端口等。
如果配置iptables不指定表名，则默认为`filter`表。
其可以配置三个动作：`ACCEPT`, `REJECT`, `DROP`。可以作用于三个链上：`input`, `output`, `forward`。
其中`ACCEPT`为接受，也就是允许连接或转发。`REJECT`为积极拒绝，TCP场景下会发送TCP RST报文，让连接方感知到该端口无法连接，但由于返回了RST报文，连接方可以得知该IP有对应设备。`DROP`为丢弃数据包而不做任何额外处理，在连接方看来无法收到有效信息，可能会被认为是主机不可达。
`filter`表主要用于`INPUT`与`FORWARD`链上，但是也可以用于`OUTPUT`链上。功能与`INPUT`类似。

举例：禁止`192.168.1.0/24`网段以外的IP访问22端口，需要注意的是前后顺序不能变，否则无法连接。
```bash
iptables -A INPUT -p tcp -s 192.168.1.0/24 --dport 22 -j ACCEPT
iptables -A INPUT -p tcp -m tcp --dport 22 -j DROP
```

### `nat`表
`nat`表用于NAT，可以完成网络地址转换功能，包括源NAT、目的NAT、地址映射等。在大多数内核下，使用该表可以实现1:1 SNAT， 1:1 DNAT， M:1 SNAT， M:1 DNAT以及两个网段之间的M:N NAT。但是仅使用`iptables`无法实现M:N或1:N的NAT，需要借助其后继者`nftables`才可实现。在实现SNAT时，会尽可能不改变源端口。而实现DNAT时，如果没有指定端口范围则不会改变源端口。NAT表依赖连接跟踪功能，如果一个连接没有连接跟踪，则无法应用NAT表中的规则。

#### 源NAT

NAT中最常用的为SNAT，这是内网访问公网时的常见形式。SNAT仅作用于`POSTROUTING`链。下面举个例子，假设内网网段为`192.168.1.0/24`，公网IP地址为`13.0.7.33/32`。
```bash
iptables -t nat -A POSTROUTING -s 192.168.0.0/24 -o eth0 -j SNAT --to-source 13.0.7.33
```
这条命令将来自`192.168.1.0/24`网段的流量的源地址均转换为13.0.7.23，并且如果可能的话不会改变端口。
##### 地址伪装
一种在家庭中更常见的情况是IP地址由运营商通过拨号等方式动态分配，那么可以使用`MASQUERADE`方式进行NAT，这种NAT方式与SNAT类似，但是它会自动使用当前网卡的实际IP地址。
```bash
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o ppp0 -j MASQUERADE
```
这里转换到的地址为网卡`ppp0`自身的地址，如果IP地址更改，其也会自动更改地址。

#### 目的NAT
目的NAT（DNAT）通常用于负载均衡，但`iptables`原生已经不再支持负载均衡式的目的NAT，所以并不是很重要。目的NAT可以作用于两个链，为`PREROUTING`和`OUTPUT`链，对于转发包，目的NAT会在`PREROUTING`链中处理，对于本机外发的包，则会在`OUTPUT`链中处理。

##### 重定向
可以将外部包重定向到本机的其他端口，可作用于`PREROUTING`和`OUTPUT`链。它可以将数据包的目的地址重定向为其入接口的主IP，可以用于端口重定向和透明代理。

#### 网络映射

网络映射是一种严格的映射，下面的命令会将`192.168.1.0/24`网段内的主机一一映射到`10.0.0.0/24`网段的主机，例如`192.168.1.193`会映射到`10.0.0.193`。如果其他网段的设备想要访问`192.168.1.193`，只需要访问`10.0.0.193`即可。
```bash
iptables -t nat -A PREROUTING -d 192.168.1.0/24 -j NETMAP --to 10.0.0.0/24
```
如果两个网段不一致，可能会出现无法访问到目标网段的主机，或者有些目标网段的主机无法被访问到的情况。
### `mangle`表
`mangle`表可以用来修改数据包的一些元信息。其能够修改包括数据包本身包含的信息，如TTL、TOS、TCP MSS等。也能数据包本身不包含，存储在本机内存中的额外标记信息。由`mangle`表修改的标记信息不仅能在`iptalbes`中使用，也可以用于路由决策。使用`mangle`表可以做到很多复杂的路由操作，但该表无法直接修改数据包的内容，如果想要修改数据包的内容，需要搭配`NFQUEUE`使用。

在`iptables`用户手册中提到了一个修改TCP MSS的例子，如果你发现SSH可以连接，但SCP无法连接，或者类似网站可以连接，但无法收到消息。类似上面的流量一大就出现问题的情况，很可能是在路由过程中有些路由设备的MTU比本机的MTU更小，且不允许IP分片，导致段内容较多的TCP无法正常连接，此时可以尝试使用`mangle`表调整TCP MSS来减小MSS，测试该问题是否为链路过程中MTU过小引起。
```bash
# 自动将MSS设置为MTU
iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN \
            -j TCPMSS --clamp-mss-to-pmtu
# 将MSS设置为500，包长度则为 500+20+20 = 540
iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN \
            -j TCPMSS --set-mss 500
```
### `raw`表

`raw`表最大的作用是用于设置`NOTRACK`标记位，作用域`PREROUTING`和`OUTPUT`，一旦一个包被设置了该位，则内核不会再对该流进行连接跟踪，此时`nat`表会彻底失去作用。


# 应用实例

## 允许任何地址访问SSH端口
```
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
```
允许访问目的端口为22的所有TCP入站连接，公网服务器默认需要配置这条，否则可能会连不上。
## 禁止非WAF服务商访问443端口
我们以Cloudflare其中的一个IP段 `103.21.244.0/22`为例。
```bash
iptables -A INPUT -p tcp -s 103.21.244.0/22 --dport 443 -j ACCEPT
iptables -A INPUT -p tcp --dport 443 -j DROP
```
由于`iptables`的规则为从上到下匹配，WAF服务商的规则要在前面，它们的流量可以命中前面的规则，允许连接，其他连接则会被静默忽略。
## 阻断长度超过1000的数据包
过滤条件可以指定数据包长度范围。当指定`--length`等过滤条件时，过滤条件会应用于整个IP数据包，所以其长度包括IP头部和TCP/UDP头部，但不包括以太网帧头部，也就是不包含MAC地址和数据包类型的长度。

```bash
iptables -A INPUT -p tcp -m tcp --dport 8885 -m length --length 1001: -j DROP
```
这条规则会丢弃目的端口为8885，包长度大于1000（不包括1000）的进入本机的数据包，但TCP负载的最大长度为960，即包括20字节的IP头和20字节的TCP头，在我的环境实测为948字节，因为我的设备本地传输时TCP头部为32字节，加上20字节的IP头，长度刚好1000字节，如果传输949字节的TCP数据就会被丢弃。

上面这条规则作用于输入链上，类似的规则也可以作用于转发链上。
```bash
iptables -A FORWARD -s 8.8.8.8/32 -p tcp -m length --length 1001: -j DROP
```
当设备转发一个TCP数据包时，如果这个数据包从IP`8.8.8.8`来，且长度大于1000，则会丢弃掉，但不会影响`8.8.8.8`到本机的数据包。

## 同网段单臂路由
同网段是不需要路由的，但是有些情况下可能会需要将一台设备的流量截取修再传回，或根据条件丢弃数据包模拟网络波动，但设备本身和路由器都不支持截取，唯一的一台Linux设备也只有一个网口时，就可以使用这种方式。
简单看下预期的网络拓扑：
客户端(192.168.1.114) <-> 本机(192.168.1.10) <-> 路由器(192.168.1.1) <-> 公网服务器(8.8.8.8)

首先在客户端上修改路由表，将路由指向本机。然后在本机启用转发功能。此时出流量会经过本机，但是回流量并不会经过本机，这是因为本机没有修改客户端的源IP，而路由器检测到客户端和路由器处于同一网段时，会直接将数据发给客户端，而不是发给本机，发生了来回链路不一致。
出方向链路：
客户端(192.168.1.114) -> 本机(192.168.1.10) -> 路由器(192.168.1.1) -> 公网服务器(8.8.8.8)
回方向链路：
客户端(192.168.1.114) <- 路由器(192.168.1.1) <- 公网服务器(8.8.8.8)

这就导致本机只能检测到去方向的包。为了解决这个问题，需要在本机增加SNAT，让客户端的流量经过本机后，源IP改为本机的IP，这样路由器才会将回包发回给本机。
```
iptables -t nat -A POSTROUTING -s 192.168.1.114 -j MASQUERADE
```

## 回放流量时禁止目标主机响应
有时需要使用`tcpreplay`回放流量测试防火墙功能，但回放时，目标主机可能会发送很多rst报文，导致防火墙不能正确建立会话表，无法正常进行测试，此时就可以在目标主机中使用`iptables`，丢弃来自回放设备的报文。
```
iptables -A INPUT -s 77.77.77.77/32 -j DROP
```
这条命令会丢弃来自`77.77.77.77`的所有报文，不会发送rst报文回去。
## TCP、UDP 转发至不同路由
这是综合使用`iptables`和路由的方式。
首先可以标记不同的数据包。这里的每个mark都只会标记数据包，而不会标记整条流，所以这里只是根据TCP和UDP这样的流量静态信息来进行分流，如果需要标记整条流，从而使得不同TCP连接也能够分流，那么需要`connmark`跟踪。
```bash
iptables -t mangle -A PREROUTING -p tcp -j MARK --set-mark 100
iptables -t mangle -A PREROUTING -p tcp -j MARK --set-mark 200
```
接下来在`/etc/iproute2/rt_tables`中配置自定义路由表。
```
100 tcp_table
101 udp_table
```
使用`ip rule` 和 `ip route`命令分别为不同的流量设置路由。
```bash
# 为路由表使用不同的网关
ip route add default via 1.1.1.1 dev eth0 table tcp_table
ip route add default via 2.2.2.2 dev eth0 table udp_table

# 为不同的标记分别使用不同的路由表
ip rule add fwmark 100 table tcp_table
ip rule add fwmark 200 table udp_table
```


# 修改数据包内容
`iptables`原生并不支持修改数据包内容，但是可以通过内核模块、eBPF或NFQUEUE等方式修改数据包内容。内核模块和eBPF由于工作在Linux内核中，所以开发语言非常受限，往往只能通过C、C++、Rust这样的可编译为无运行时可执行二进制的语言编写（所以Go不行，但是Lua行，没想到吧），其他语言编写内核相关模块会非常复杂。另外内核态程序一旦出现错误，很可能引起内核panic，引发整个系统崩溃，所以通常不建议使用内核模块扩展`netfilter`功能。

不过内核还提供了`NFQUEUE`来帮助我们绕过内核，在用户态控制网络数据包。
`NFQUEUE`的核心逻辑是将内核中的数据包复制到用户态程序中，并且由用户态程序通知内核数据包的去向。在这个过程中，用户态程序可以对数据包进行任何修改，并将修改后的数据包传回内核。可以使用`scapy`、`netfilterqueue`结合`NFQUEUE`模块，使用Python在用户态运行这项功能。这种方式灵活性非常高，在做小规模的PoC测试时非常实用。

```bash
sudo iptables -I INPUT  -p tcp --dport 8085 -j NFQUEUE --queue-num 0
sudo iptables -I OUTPUT -p tcp --sport 8085 -j NFQUEUE --queue-num 0
```


```python
from netfilterqueue import NetfilterQueue
from scapy.all import *

def process_packet(packet):
    # 将数据包从 nfqueue 提取出来
    pkt = IP(packet.get_payload())
    
    # 打印数据包内容（用于调试）
    print(pkt.summary())
    
    # 修改数据包内容 (例如，修改数据包负载)
    if pkt.haslayer(TCP):
        pkt[TCP].payload = Raw(load=b'Changed payload')
    
    # 将修改后的数据包重新放入 nfqueue
    packet.set_payload(bytes(pkt))
    packet.accept()

def main():
    nfqueue = NetfilterQueue()
    nfqueue.bind(0, process_packet)
    
    try:
        print("Starting packet processing...")
        nfqueue.run()
    except KeyboardInterrupt:
        print("Stopping packet processing...")
    finally:
        nfqueue.unbind()

if __name__ == "__main__":
    main()
```


# 网络命名空间

网络命名空间是 Linux 内核的一项功能，提供网络栈的隔离，允许 **每个网络命名空间拥有自己的独立网络配置**、接口、IP 地址、路由表和防火墙规则。

当创建一个新的网络命名空间时，它将以**完全隔离的网络栈**开始，除了回环接口 (lo) 外**没有网络接口**，这意味着在新的网络命名空间中运行的进程默认无法与其他命名空间或主机系统中的进程通信。

也可以通过这种方式在在一个操作系统内实现无需创建容器或虚拟机，就可以虚拟多个网络设备。可以用于简化一些情况下的验证。也可以用于测试可能导致无法联网的`iptables`命令，当处于网络命名空间内部时，对其的修改不会影响到本机网络，并且由于相当于直接连接tty，所以不会出现命令敲下后无法连接的问题。
