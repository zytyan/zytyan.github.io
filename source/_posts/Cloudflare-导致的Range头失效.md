---
title: Cloudflare 导致的Range头失效
date: 2022-04-06 16:24:20
tags: [Cloudflare , 建站, HTTP]
---
# Cloudflare引起的问题
在配置服务器上的Nginx时，发现无论怎么配置都无法分段下载，并且并没有返回错误，通过 `curl -I -R "Range: byte=100-200" https://my.domain.com/files/xxxx.mp4`，服务器返回的的HTTP头也并非206而是200，而在服务器后台查看 `access.log` 发现log的返回值为200，而正常的206，多次修改配置无果后，我怀疑是其他中间件导致的问题。     
由于我启用了Cloudflare反向代理，它自然成为了我的第一个怀疑对象。起初我的怀疑原因很简单，可能是由于Cloudflare的缓存没有刷新导致的问题。  
于是打开了cf的开发模式，继续尝试，发现可以使用IDM分段下载了，同样的curl命令也会返回206了，于是我清除缓存后关闭开发模式继续尝试，发现又不能分段下载了。  
## 实际原因
Cloudflare确实会缓存文件，然而根据客户协议，除非单独购买，否则一部分文件不会缓存（类似视频等），而这是通过扩展名进行判断的，所以URL的扩展名一定条件后就会触发这个机制，让Cloudflare丢弃客户端请求的Ragne头，服务端看不到，自然也就无法支持分段下载。  
然而这是我根据社区问答中的只言片语和实验猜测出来的结果，我并没有找到描述其实际行为的文档。
### 参考
[Cldouflare社区问答](https://community.cloudflare.com/t/cloudflare-keep-removing-my-range-header-randomly/298444)
[服务条款](https://www.cloudflare.com/en-gb/terms/) 在该服务条款的2.8节
---
服务条款2.8的英文原文
<details>
The Services are offered primarily as a platform to cache and serve web pages and websites. Unless explicitly included as part of a Paid Service purchased by you, you agree to use the Services solely for the purpose of (i) serving web pages as viewed through a web browser or other functionally equivalent applications, including rendering Hypertext Markup Language (HTML) or other functional equivalents, and (ii) serving web APIs subject to the restrictions set forth in this Section 2.8. Use of the Services for serving video or a disproportionate percentage of pictures, audio files, or other non-HTML content is prohibited, unless purchased separately as part of a Paid Service or expressly allowed under our Supplemental Terms for a specific Service. If we determine you have breached this Section 2.8, we may immediately suspend or restrict your use of the Services, or limit End User access to certain of your resources through the Services.
</details>
大意：服务主要作为缓存和服务网页和网站的平台提供。 除非明确包含在您购买的付费服务中，否则您同意仅将服务用于 (i) 提供通过网络浏览器或其他功能等效应用程序查看的网页，包括呈现超文本标记语言 (HTML) 或其他功能等效物，以及 (ii) 提供受本第 2.8 节规定的限制的 Web API。 禁止使用服务来提供视频或不成比例的图片、音频文件或其他非 HTML 内容，除非作为付费服务的一部分单独购买或根据我们针对特定服务的补充条款明确允许。 如果我们确定您违反了第 2.8 条，我们可能会立即暂停或限制您对服务的使用，或限制最终用户通过服务访问您的某些资源。  

## 解决方法
由于请求在转发至服务器的时候Range标头就被丢弃了，这使得我可能无法通过Cache-Control的响应头来控制缓存行为（未实验），所以我选择使用路径规则，当匹配部分字符串时则绕过缓存。
具体做法为登录cf中登录网站的控制台，在 规则 > 页面规则 中选择创建页面规则，配置需要的路径后将缓存级别设为绕过。
