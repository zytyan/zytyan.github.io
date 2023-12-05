---
title: VSCode 远程开发技巧
date: 2022-04-05 05:47:40
tags: [VSCode ,IDE,编程]
---
在越来越多的插件的加持下，VSCode的功能逐渐变得强大起来，虽然在Python代码提示、高亮和静态检查上不如jetbrains家的IDE，但是jetbrains的IDE想要进行远程开发，必须要购买专业版，此时拥有免费的远程开发插件则让VSCode成为了我远程开发的首选。本文只记录在Windows上通过SSH连接Linux进行开发的部分技巧及备忘。

# 首次配置
配置VSCode远程开发，首先需要安装微软提供的远程开发插件 [Remote - SSH](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-ssh)，安装完成后，如果是win10以下的系统需要安装OpenSSH，而Win10自带，不需要手动安装。
## 配置文件
配置文件是SSH连接的核心文件，VSCode需要根据配置文件查找远程主机并连接，配置文件的格式与SSH config文件格式相同，使用`#`作为注释的开头，我仅列举我常用的一些字段。
1. Host: 主机名称，一个备忘的名字，空格需要引号，有特殊字符。
2. HostName: 主机地址，IP或者域名。
3. User: 登录的用户。
4. IdentityFile: 使用密钥对方式登录时的私钥。
5. ProxyCommand: 如果使用代理命令需要该选项。
6. Port: 如果要连接的主机SSH端口为非常规的22端口，则应该设置Port字段。
一个常见的配置文件样式
```
Host my_lan_host
    HostName 192.168.1.10
    User daenerys
    Port 7654
    IdentityFile ~/.ssh/lan_host.key

Host tencent_cloud
    HostName 100.100.100.100
    User root
    IdentityFile ~/.ssh/tc.key

Host american_host
    HostName 99.99.99.99
    ProxyCommand "C:\bin\nmap-7.70\ncat.exe" --proxy-type socks5 --proxy 127.0.0.1:1080 %h %p 
    User adminadmin

    
```
### 配置文件权限问题
在默认情况下，VSCode使用OpenSSH的config文件，路径是`%USERPROFILE%\.ssh\config`，而如果你在此前没有使用过OpenSSH，则可能会会有一些权限上的问题，VSCode并不检查该文件的权限，而OpenSSH则会检查权限，如果该文件的权限不符合要求则不会使用该文件。此时会出现如下的错误提示。
```bat
C:\Users\your_user_name>ssh 10.0.100.100 rem 这里当然是你要连接的ip
Bad owner or permissions on C:\\Users\\your_user_name/.ssh/config
```
此时有两种解决方式，一种方式是修改config文件的权限，另一种是修改VSCode配置。
- 解决方式1  
  找到该文件后，右键属性 > 安全 > 高级，选择禁用继承，在弹出的对话框里选择从此对象中删除所有已继承的权限，此时会显示 **所有组或用户均不具有访问此对象的权限。但是对象的所有者可以分配权限。**  
  此时点击添加，选择主体，高级，立即查找，在下方的用户中找到你当前的用户，确认后为其分配完全控制权限即可。
- 解决方式2
  可能你并不喜欢修改权限，Windows权限复杂又难以更改，那么可以不修改文件权限，而直接修改VSCode所使用的配置文件即可绕过这一问题。不过如果要配置无密码访问的话修改权限是绕不过的。
  具体做法很简单，找到Remote - SSH插件，点击右下角的设置，第一个选项 `Remote.SSH: Config File`就是所使用的配置文件了，修改为另一个配置文件即可。


### 使用代理连接你的主机
并不是所有时候SSH的连接都是通畅的，比如在内网中只能使用HTTP代理来连接公网，或者服务器IP被墙等多种原因，都会让你不得不使用代理来连接，然而遗憾的是，OpenSSH并不原生支持使用常规的HTTP或Socks代理连接，只能使用SSH跳板进行连接。
但是OpenSSH可以使用代理命令，原理则是将原本的SocketIO转为代理程序的stdio，在Linux上可以使用netcat(nc)进行代理，而在Windows上就需要手动下载netcat for Windows，并将绝对路径传给ProxyCommand或将ncat加入环境变量，我此处使用绝对路径。以上面的配置文件为例。
```
Host american_host
    HostName 99.99.99.99
    ProxyCommand "C:\bin\nmap-7.70\ncat.exe" --proxy-type socks5 --proxy 127.0.0.1:1080 %h %p 
    User adminadmin
    
```
其中的特殊字段`%h`和`%p`，在ssh运行时，这两个字段将会自动被替换为`HostName` 和 `Port`，所以实际上则是启动一个`nc --proxy-type socks5 --proxy 127.0.0.1:1080 99.99.99.99 22`的程序，并将原本的SocketIO转为该程序的stdio。故你也可以用自己实现的任何其他手段来进行流量转发，而不局限于netcat。
# 终端配置
默认状态下的终端虽然可用，但对我来说用起来并不是非常方便，还需要进行进一步的自定义配置。
在终端运行程序时，常常需要用到很多ctrl组合键，然而部分组合键并不会转发至shell，而是会激活VSCode相关的功能，尤其是我使用的是IntellJ的键位，问题更加明显，此时需要将`Terminal › Integrated: Allow Chords`设置为true 。
在启用了鼠标功能的tmux中，鼠标的滑动会被汇报至shell，此时无法正常选择复制。可以按住shift键进行选择和点击（注：点击过程中**不能**松开shift）
对于右键行为，可以在`Terminal › Integrated: Right Click Behavior`进行更改。