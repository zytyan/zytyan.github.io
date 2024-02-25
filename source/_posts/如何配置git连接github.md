---
title: 如何配置git连接github
date: 2024-02-25 17:36:19
tags:
---

a



## Git如何连接GitHub

### 第一步 配置git

```shell
git config --global user.name "Your Name" # 可以是任何名字
# 这里的Email可以是任何邮箱，但是只有在GitHub上配置过的邮箱（默认是登陆邮箱）
# 才能看到你的小绿块（活动记录）
git config --global user.email your_github_email@gmail.com
```

可以看到git只配置了邮箱和名字，并没有密码之类的身份验证的东西。事实上，git并不做身份验证，只有git使用的SSH或HTTPS才可能需要身份验证。因为git完全可以在单机或局域网内运行，所以身份验证对于git来说并无必要。

不过GitHub需要知道你的身份，因为GitHub需要管理权限、付费等等必要因素，而git又是匿名的，所以需要使用一种身份验证方式来确认身份，如果你使用HTTPS进行clone操作的话，那么每次提交都会让你输入GitHub的用户名密码，所以可以使用SSH进行clone，这样我们可以用SSH来登录GitHub，通过下面的操作，GitHub会将某个SSH密钥对关联到你的账号，无需像HTTPS那样频繁输入用户名密码。

### 第二步 生成密钥对

>  **注意**：使用下面的命令时千万要注意输出中的`Overwrite`（覆盖）这个关键字，出现该关键字说明你已经有一个私钥了，就算无脑按回车（默认会跳过）也不要无脑按y继续。

```shell
# 这里的email并不一定要与上面的相同，同样也可以是任何邮箱
# 而且邮箱不会影响使用，随便编一个也是可以的，但是太多不好管理
ssh-keygen -C "your_github_email@gmail.com"
```

命令会询问你输入 passphrase，我们不管，直接回车就好。

默认情况下，`ssh-keygen`命令会在`~/.ssh/`目录下生成`id_rsa`和`id_rsa.pub`两个文件。

Windows下可以通过复制`%UserProfile%\.ssh\`到资源管理器访问文件夹，Linux下可以使用`cd ~/.ssh`访问。

也可以使用`-f`选项指定路径，如果指定那么则是`path/to/name`和`path/to/name.pub`两个文件。

### 第三步 将公钥添加到GitHub

前往上一步生成的目录，使用文本编辑器打开带有`.pub`后缀的文件，会看到一行文字，这就是要复制到GitHub上的SSH公钥。其类似下面这样

```
ssh-rsa AAAAC3...... your_github_email@gmail.com
```

复制该行内容到[Github SSH and GPG keys 页面](https://github.com/settings/keys)，点击 New SSH Key，Title随便输入一点什么，Key则为刚刚复制的那行文字。



### 第四步 配置SSH

如果在第二步生成密钥对时没有使用`-f`指定文件名，那么该步骤可以忽略，请直接到下一步。

有两种方法可以配置，使用其一即可。

- 第一种方法

  使用文本编辑器打开`~/.ssh/config`文件，在最后面添加如下行。

  ```
  Host github.com
      HostName github.com
      User git
      IdentityFile path/to/your/private_key # 请将这里替换为你的私钥
      IdentitiesOnly yes
  ```

  `~/.ssh/config`文件为ssh使用的配置文件，在这里使用即可让连接到GitHub的命令均使用`IdentityFile`指定的私钥。

  这种方法是全局生效的，并且对第二种方法有隐性影响。

- 第二种方法

  在命令行中输入以下命令，注意路径不要有空格等特殊字符，不然引号转义很麻烦。

  ```shell
  git config --global core.sshCommand 'ssh -i path/to/your/private_key'
  ```

  这种方式也是全局生效的，不过如果使用了第一种方法配置后，这里的文件使用错误，则会默认使用第一种方法下的`IdentityFile`，而不会报错。

  如果去掉命令中的`--global`则可只对某个仓生效，这对使用多个GitHub账号的人而言会方便一些。

### 第五步 测试

输入以下命令

```shell
ssh -T git@github.com
```

如果出现下面的输出，那么就做对了。

```
Hi XXX! You've successfully authenticated, but GitHub does not provide shell access.
```

如果出现下面的输出，那么就是你的公钥没有配置对，需要重新从第二步开始检查。

```
git@github.com: Permission denied (publickey).
```



## 使用GPG签名提交

这里并非必选项，如果只需要连接到GitHub，这里可以不用看了。

在第一步我们发现我们可以随意填写名字和邮箱时，git的身份是很容易伪造的，在互联网下进行合作时，可以使用GPG对自己的提交进行签名，以避免其他人伪造自己的提交。对于初学GitHub的普通人而言，自然没人会伪造你，不过如果公司有要求，或者你意外出名了，就可以使用GPG的方式来签名你的提交。

GPG全程为GNU Privacy Guard，虽然直译为隐私保护，但目前它的主要用法是防止伪造身份。与实名制不同，这种方式下使用者身份只与一个私钥文件挂钩，而不与任何现实身份挂钩，只要私钥不泄露，身份就不会被伪造，同时也可以保证隐私。可以认为谁有这个私钥，谁就有这个身份。

这里的教程只教GPG的最基本用法，GPG的套路可以很复杂，但这里进行了尽可能的简化。

### 第一步 安装GPG

没什么好说的，`apt`、`yum`、`brew` 之类的命令install就行，Windows用户可以安装[Gpg4win](https://www.gpg4win.org/thanks-for-download.html)。不会可以去Google，这不是本文章的重点。

对于**Windows**用户，需要额外多执行一条命令以使得Git适配GPG，这是因为安装Git时，其自带的MinGW环境自带了一个MinGW版GPG，这会导致git优先使用自带的GPG，而非你安装的GPG，当然，你也可以通过调整`PATH`环境变量的方式直接使用git自带的GPG，应该也是一种可行的方案。Linux / Mac 用户不需要。

```powershell
where.exe gpg
# 在获得了目录以后，需要将下面的字符串换成where输出的位置。
git config --global gpg.program "C:\Program Files (x86)\GnuPG\bin\gpg.exe"
```

### 第二步 生成密钥对

很遗憾，GPG并不能直接使用SSH的私钥，所以除了上面的SSH密钥，还需要额外管理一套GPG密钥。相比SSH密钥，GPG密钥的自定义项会比较多。

使用`gpg --full-generate-key`生成一个新密钥对，这会交互式地引导用户创建一个密钥对。

过程中会要求输入名字和邮箱，这里的邮箱必须是一个你能够收到邮件的邮箱，因为GitHub要使用该邮箱做认证。

中间可能需要你输入`passphrase`，和SSH私钥类似，我们也可以不输入直接点击OK（Windows，Linux下回车），这样可以创建一个无密码保护的密钥。虽然理论上无密码保护会导致黑客攻破设备时私钥被直接窃取，但以我的经验看，比起电脑被黑客攻击的概率，密码把未来的我自己防住的概率要高得多得多，所以除非有足够的准备，否则不建议大家输入密码，不然将来会后悔的。



剩下的去看GitHub去吧，没什么大坑了。

[Telling Git about your signing key - GitHub Docs](https://docs.github.com/en/authentication/managing-commit-signature-verification/telling-git-about-your-signing-key)
