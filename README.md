# Distributed Key-Value Storage
**分布式键值存储**

## gRPC简介
**什么是 RPC?**

**RPC是Remote Procedure Call的简称，中文叫远程过程调用。**

可以这么理解：现在有两台服务器A和B。部署在A服务器上的应用，想调用部署在B服务器上的另一个应用提供的方法，由于不在一个内存空间，不能直接调用，需要通过网络来达到调用的效果。

现在，我们在A服务的一个本地方法中封装调用B的逻辑，然后只需要在本地使用这个方法，就达到了调用B的效果。
对使用者来说，屏蔽了细节。你只需要知道调用这个方法返回的结果，而无需关注底层逻辑。

从封装的那个方法角度来看，调用B之前我们需要知道什么？
当然是一些约定啊。比如，
•	调用的语义，也可以理解为接口规范。(比如RESTful)
•	网络传输协议 (比如HTTP)
•	数据序列化反序列化规范(比如JSON)。

有了这些约定，我就知道如何给你发数据，发什么样的数据，你返回给我的又是什么样的数据。
![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/b5f184e8-4acd-4161-8c09-2b6f68fe1440)

从上图中可以看出，RPC是一种客户端-服务端（Client/Server）模式。
从某种角度来看，所有本身应用程序之外的调用都可以归类为RPC。无论是微服务、第三方HTTP接口，还是读写数据库中间件Mysql、Redis。
**HTTP 和 RPC 有什么区别？**

HTTP只是一个通信协议，工作在OSI第七层。
而RPC是一个完整的远程调用方案。它包含了:接口规范、传输协议、数据序列化反序列化规范。

**RPC 和 gRPC 有什么关系？**

gRPC是由 google开发的一个高性能、通用的开源RPC框架，主要面向移动应用开发且基于HTTP/2协议标准而设计，同时支持大多数流行的编程语言。
gRPC基于 HTTP/2协议传输。而HTTP/2相比HTTP1.x，有以下一些优势:

**用于数据传输的二进制分帧**

HTTP/2采用二进制格式传输协议，而非HTTP/1.x的文本格式。

 ![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/8fe625f3-7c20-45c9-bc7b-a13e6d9c92fe)

**多路复用**

HTTP/2支持通过同一个连接发送多个并发的请求。
而HTTP/1.x虽然通过pipeline也能并发请求，但多个请求之间的响应依然会被阻塞。

![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/389219e3-0375-4bd2-ab2f-ce4b09a855c3)


**服务端推送**

服务端推送是一种在客户端请求之前发送数据的机制。在HTTP/2中，服务器可以对客户端的一个请求发送多个响应。而不像HTTP/1.X一样，只能通过客户端发起request,服务端才产生对应的response。

**减少网络流量的头部压缩。**

HTTP/2对消息头进行了压缩传输，能够节省消息头占用的网络流量。

### **gRPC 是如何进行远程调用的?**

 ![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/41810158-dab5-4def-ab5c-61d497c7fa34)

从上图和文档中可以看出，用gRPC来进行远程调用服务，客户端(client) 仅仅需要gRPC Stub(存根) ，通过Proto Request向gRPC Server发起服务调用，然后 gRPC Server通过Proto Response(s)将调用结果返回给调用的client。
 
![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/b6be0efd-da6a-4fc4-a29a-54e6d6e28df0)


## 代码分析

#### id_distance
这段代码定义了一个名为 `id_distance` 的函数，它用于计算两个64位整数 `xId` 和 `yId` 之间的异或距离。具体功能如下：

```cpp
uint64_t id_distance(uint64_t xId, uint64_t yId)
{
    return xId ^ yId;
}
```

这个函数简单地将 `xId` 和 `yId` 进行异或操作（XOR），并返回结果。异或操作对应位相同为0，不同为1。

示例演示：

假设 `xId` 为 `0x1234567890ABCDEF`，`yId` 为 `0xFEDCBA0987654321`，它们的二进制表示分别如下：

```
xId: 0001001000110100010101100111100001001000101011111101011111011111
yId: 1111111011011100101100100000100010011001011000100100001100100001
```

将它们进行异或操作：

```
result: 1110110011101000111001000111000011010001110010011101010011011110
```

**这是两个64位整数之间的异或结果，表示它们之间的距离。这种距离计算方法通常在分布式哈希表和分布式系统中用于确定节点之间的距离或关系。**



```shell
client connect fail
14: failed to connect to all addresses
Greeter received: RPC failed

需要关闭代理，unset http_proxy

```
