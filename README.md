# Distributed Key-Value Storage
**分布式键值存储**

## Kademlia分布式哈希算法

### DHT

  分布式哈希表（Distributed Hash Table）是一种分布式存储数据的方法，它将数据分散存储在网络中的多个节点上，每个节点负责管理一部分数据。DHT 允许用户通过一个分布式网络来查找、存储和检索数据，而无需集中式的管理或索引服务器。<br/>
  
设想一个场景：有一所1000人的学校，现在学校突然决定拆掉图书馆（不设立中心化的服务器），将图书馆里所有的书都分发到每位学生手上（所有的文件分散存储在各个节点上）。那么所有的学生，共同组成了一个分布式的图书馆。

![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/76dbd0e9-4545-4e05-ae26-12538070e78f)

### Kademlia

  Kademlia是分布式哈希表（Distributed Hash Table, DHT）的一种。而DHT是一类去中心化的分布式系统。<br/>
在这类系统中，每个节点（node）分别维护一部分的存储内容以及其他节点的路由/地址，使得网络中任何参与者（即节点）发生变更（进入/退出）时，对整个网络造成的影响最小。<br/>

  Kad 使用 160bit的散列算法，完整的 key 用二进制表示有 160 个数位（bit）。实际运行的 Kad 网络，即使有几百万个节点，相比 keyspace（2ˇ160）也只是很小很小很小的一个子集。其次，由于散列函数的特点，key 的分布是高度随机的。因此任何两个 key 都不会非常临近。<br/>
![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/ad91e9da-f149-417b-9889-0592a59e4757)

### 拓扑结构：二叉树
Kademlia采用了“node ID 与 data key 同构”的设计思路。Kademlia 采用某种算法把 key 映射到一个二叉树，每一个 key 都是这个二叉树的叶子。在映射之前，先做以下预处理。<br/>
 - 先把 key 以二进制形式表示，然后从高位到低位依次处理；
 - 二进制的第 n 个 bit 就对应了二叉树的第 n 层；
 - 如果该位是 1，进入左子树，是 0 则进入右子树；
 - 把每一个 key 缩短为它的最短唯一前缀。

![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/35c6f024-2dcb-47d2-ad69-0726a115fe6f)

### 二叉树拆分
**二叉树拆分：** 对每一个节点，都可以按照自己的视角对整个二叉树进行拆分。<br/>
**拆分规则：** 先从根节点开始，把不包含自己的那个子树拆分出来；然后在剩下的子树再拆分不包含自己的下一层子树；以此类推，直到最后只剩下自己。<br/>
![image](https://github.com/lus-oa/Distributed-Key_Value-Storage/assets/122666739/d239d128-0fe1-4010-ab46-84bd6fe9b5e8)

  Kademlia 默认的散列值空间是 m = 160（散列值有 160 bits），因此拆分出来的子树最多有 160 个（考虑到实际的节点数远远小于2^160，子树的个数会明显小于 160）。对于每一个节点而言，当它以自己的视角完成子树拆分后，会得到 n 个子树；对于每个子树，如果它都能知道里面的一个节点，那么它就可以利用这 n 个节点进行递归路由，从而到达整个二叉树的任何一个节点。


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



