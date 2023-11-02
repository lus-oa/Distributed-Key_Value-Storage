# Distributed Key-Value Storage
**分布式键值存储**

## gRPC简介

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
