/*
 * node.hpp
 *
 */

#ifndef INCLUDE_NODEKADIMPL_HPP_
#define INCLUDE_NODEKADIMPL_HPP_

#include <map>
#include <deque>
#include <iostream>
#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>

#include <math.h>
#include <pthread.h>

#include "proto/dhash.pb.h"
#include "proto/dhash.grpc.pb.h"

template <class K, class V>
using map = std::unordered_map<K, V>;

template <class T>
using set = std::unordered_set<T>;

template <class T>
using vector = std::vector<T>;

template <class T>
using deque = std::deque<T>;

uint64_t id_distance(uint64_t xId, uint64_t yId)
{
	return xId ^ yId;
}

// 这个函数用于计算输入的无符号 64 位整数在二进制表示中从最高位到最低位的距离，即第一个 1 之前的 0 的个数
uint64_t k_id_distance(uint64_t dis)
{
	int i = 0;
	while (dis)
	{
		dis = dis >> 1;
		i = i + 1;
	}
	return i - 1;
}

// 将字符串中的二进制数据按照字节进行复制，并将其解释为一个 64 位无符号整数。
uint64_t str2u64(const std::string data)
{
	uint64_t ret;
	memcpy(&ret, data.c_str(), sizeof(uint64_t));
	return ret;
}

namespace std
{
	template <>
	struct hash<Node>
	{
		size_t operator()(const Node &__x) const
		{
			return __x.id();
		}
	};
}

// 定义一个名为 Lock 的类
class Lock
{
	// 用于存储一组互斥锁的指针
	pthread_mutex_t *lock_;
	// 记录互斥锁的数量
	int num_locks;

public:
	// 构造函数，接受一个整数参数 num，用于指定要创建的互斥锁的数量
	Lock(int num)
	{
		// 将传入的数量赋值给成员变量 num_locks
		num_locks = num;
		// 动态分配一组互斥锁的内存，并将指针存储在成员变量 lock_ 中
		lock_ = new pthread_mutex_t[num_locks];
		// 循环初始化每个互斥锁
		for (int i = 0; i < num_locks; i++)
		{
			// 使用 PTHREAD_MUTEX_INITIALIZER 初始化互斥锁
			lock_[i] = PTHREAD_MUTEX_INITIALIZER;
		}
	}

	// lock 方法，接受一个整数参数 k，用于指定要锁定的互斥锁的索引
	void lock(int k)
	{
		// 如果传入的索引 k 大于等于互斥锁的数量
		if (k >= num_locks)
		{
			// 重置索引 k 为 0，以防止越界访问
			k = 0;
		}

		// 使用 pthread_mutex_lock 锁定指定索引 k 的互斥锁
		pthread_mutex_lock(lock_ + k);
	}

	// unlock 方法，接受一个整数参数 k，用于指定要解锁的互斥锁的索引
	void unlock(int k)
	{
		// 如果传入的索引 k 大于等于互斥锁的数量
		if (k >= num_locks)
		{
			// 重置索引 k 为 0，以防止越界访问
			k = 0;
		}
		// 使用 pthread_mutex_unlock 解锁指定索引 k 的互斥锁
		pthread_mutex_unlock(lock_ + k);
	}
};

class NodeKadImpl : public KadImpl::Service
{

	using Status = grpc::Status;							// 使用别名 Status 代表 grpc::Status 类型
	using ServerContext = grpc::ServerContext;				// 使用别名 ServerContext 代表 grpc::ServerContext 类型
	using ClientContext = grpc::ClientContext;				// 使用别名 ClientContext 代表 grpc::ClientContext 类型
	using Nodes = google::protobuf::RepeatedPtrField<Node>; // 使用别名 Nodes 代表 google::protobuf::RepeatedPtrField<Node> 类型
	std::string local_address = "";							// 字符串类型变量 local_address，用于存储本地地址
	uint64_t local_nodeId = 0;								// 64 位无符号整数变量 local_nodeId，用于存储本地节点的唯一标识
	uint64_t k_closest = 2;									// 64 位无符号整数变量 k_closest，用于表示 k-最近邻（k-closest）的数量
	uint64_t num_buckets = 4;								// 64 位无符号整数变量 num_buckets，用于表示桶的数量
	Node local_node;										// Node 类型变量 local_node，用于存储本地节点的信息
	deque<Node> **nodetable;								// 双端队列（deque）指针数组 nodetable，用于表示节点表，需要设计
	vector<Node> *sbuff_, *cbuff_;							// Node 类型指针数组 sbuff_ 和 cbuff_，用于存储节点信息的缓冲区
	map<uint64_t, uint64_t> *_db;							// 64 位整数对的映射，用于表示数据库
	Lock *lock;												// Lock 类型指针变量 lock，用于管理互斥锁

public:
	// NodeKadImpl 构造函数，接受地址、节点ID和 k-最近邻的参数
	NodeKadImpl(std::string address, uint64_t id, uint64_t k = 2)
	{
		// 将传入的地址存储到本地地址变量 local_address
		local_address = address;
		// 将传入的节点ID存储到本地节点ID变量 local_nodeId
		local_nodeId = id;
		// 设置本地节点的地址为传入的地址
		local_node.set_address(local_address);
		// 设置本地节点的ID为传入的节点ID
		local_node.set_id(local_nodeId);
		// 设置 k_closest 变量为传入的 k 值
		k_closest = k;
		// 根据 k 值计算 num_buckets（桶的数量），并存储到 num_buckets 变量
		num_buckets = pow(2, k);
		// 创建一个 Lock 对象，用于管理互斥锁，锁的数量为 num_buckets
		lock = new Lock(num_buckets);
		// 动态分配存储节点信息的双端队列指针数组 nodetable
		nodetable = new deque<Node> *[num_buckets];
		// 初始化 nodetable 中的每个桶（deque）
		for (int i = 0; i < num_buckets; i++)
		{
			nodetable[i] = new deque<Node>();
		}
		// 动态分配一个映射表，用于表示数据库
		_db = new map<uint64_t, uint64_t>();
		// 动态分配存储节点信息的向量 sbuff_
		sbuff_ = new vector<Node>();
		// 动态分配存储节点信息的向量 cbuff_
		cbuff_ = new vector<Node>();
	}

	// 函数 find_node 用于处理查找节点操作，接收 gRPC 请求并返回 gRPC 响应
	Status find_node(ServerContext *context, const IDKey *request, NodeList *response)
	{
		// 打印调试信息，显示当前节点的唯一标识
		printf("find_node 1 %lu\n", local_nodeId);

		// 解析请求中的目标 ID，并将其转换为 64 位整数
		uint64_t target_id = str2u64(request->idkey);

		// 调用 findCloseById 函数查找最接近目标 ID 的节点
		deque<Node> nodes = findCloseById(target_id);

		// 将本地节点的信息添加到响应中
		response->mutable_resp_node()->CopyFrom(local_node);

		// 打印调试信息，显示当前节点的唯一标识
		printf("find_node 2 %lu\n", local_nodeId);

		// 将查找到的节点信息添加到响应中
		for (Node node_ : nodes)
		{
			Node *node = response->add_nodes();
			node->set_address(node_.address());
			node->set_id(node_.id());
		}

		// 打印调试信息，显示当前节点的唯一标识
		printf("find_node 3 %lu\n", local_nodeId);

		// 调用 freshNode 函数，用于更新节点信息
		freshNode(request->node());

		// 返回 gRPC OK 状态，表示操作成功
		return Status::OK;
	}

	// 函数 find_value 用于处理查找键值对操作，接收 gRPC 请求并返回 gRPC 响应
	Status find_value(ServerContext *context, const IDKey *request, KV_Node_Wrapper *response)
	{
		// 从请求中提取键（key）并将其转换为 64 位整数
		uint64_t key = str2u64(request->idkey());

		// 在数据库中查找键对应的值
		auto iter = _db->find(key);

		// 将本地节点的信息添加到响应中
		response->mutable_resp_node()->CopyFrom(local_node);

		// 如果在数据库中找到了对应键的值
		if (iter != _db->end())
		{
			// 获取键值对的值
			uint64_t value = iter->second;

			// 将响应模式设置为键值对模式
			response->set_mode_kv(true);

			// 创建 KeyValue 消息，包含键和值的信息
			KeyValue kv;
			kv.mutable_node()->CopyFrom(local_node);

			// 将键和值分别设置为二进制数据
			kv.set_key((char *)(&key), sizeof(uint64_t));
			kv.set_value((char *)(&value), sizeof(uint64_t));

			// 将 KeyValue 消息添加到响应中
			response->mutable_kv()->CopyFrom(kv);
		}
		else
		{
			// 如果在数据库中未找到对应键的值，将响应模式设置为非键值对模式
			response->set_mode_kv(false);

			// 查找最接近键的节点
			deque<Node> nodes = findCloseById(key);

			// 将这些节点的信息添加到响应中
			for (Node node_ : nodes)
			{
				Node *node = response->add_nodes();
				node->set_address(node_.address());
				node->set_id(node_.id());
			}
		}

		// 调用 freshNode 函数，用于更新节点信息
		freshNode(request->node());

		// 返回 gRPC OK 状态，表示操作成功
		return Status::OK;
	}

	// 函数 store 用于处理存储键值对操作，接收 gRPC 请求并返回 gRPC 响应
	Status store(ServerContext *context, const KeyValue *request, IDKey *response)
	{
		// 从请求中提取键和值，并将它们转换为 64 位整数
		uint64_t key = str2u64(request->key());
		uint64_t value = str2u64(request->value());

		// 将键值对存储到数据库中
		(*_db)[key] = value;

		// 调用 freshNode 函数，用于更新节点信息
		freshNode(request->node());

		// 将本地节点的唯一标识添加到响应中
		response->set_idkey((char *)(&local_nodeId), sizeof(uint64_t));

		// 将本地节点的信息添加到响应中
		response->mutable_node()->CopyFrom(local_node);

		// 返回 gRPC OK 状态，表示操作成功
		return Status::OK;
	}

	// 函数 exit 用于处理退出节点操作，接收 gRPC 请求并返回 gRPC 响应
	Status exit(ServerContext *context, const IDKey *request, IDKey *response)
	{
		// 从请求中提取目标 ID，并将其转换为 64 位整数
		uint64_t target_id = str2u64(request->idkey());

		// 调用 removeById 函数，用于从系统中删除指定 ID 的节点
		removeById(target_id);

		// 将本地节点的唯一标识添加到响应中
		response->set_idkey((char *)(&local_nodeId), sizeof(uint64_t));

		// 将本地节点的信息添加到响应中
		response->mutable_node()->CopyFrom(local_node);

		// 返回 gRPC OK 状态，表示操作成功
		return Status::OK;
	}

	void join(std::string address)
	{
		// 创建 gRPC 通道，连接到指定的地址
		auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
		// 创建 KadImpl 服务的存根（Stub）对象
		std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
		// 创建客户端上下文
		ClientContext context;
		// 创建 IDKey 请求消息，用于发起节点加入操作
		IDKey request;
		// 设置 IDKey 消息中的 idkey 字段为本地节点的唯一标识
		request.set_idkey((char *)(&local_nodeId), sizeof(uint64_t));
		// 将本地节点信息添加到请求消息中
		request.mutable_node()->CopyFrom(local_node);
		// 创建 NodeList 响应消息，用于接收远程节点的响应
		NodeList response;
		// 调用 find_node RPC 方法，发起节点查找操作，并获取状态
		Status status = stub->find_node(&context, request, &response);
		// 从响应中获取响应节点信息
		Node resp_node = response.resp_node();
		// 从响应中获取远程节点列表
		Nodes remote_nodes = response.nodes();
#ifdef DHASH_DEBUG
		// 打印节点表的调试信息
		printNodeTable();
#endif
		// 调用 freshNode 函数，用于更新本地节点信息
		freshNode(resp_node);
		// 遍历远程节点列表，调用 freshNode 函数更新远程节点信息
		for (const auto &node : remote_nodes)
		{
			freshNode(node);
		}
#ifdef DHASH_DEBUG
		// 打印节点表的调试信息
		printNodeTable();
#endif
	}

	/*
	 * bool get(uint64_t key, uint64_t &value)
	 * 这个函数的主要目的是在接收到查找值的请求后，根据目标键查找键值对的值。
	 * 如果在本地数据库找到，则直接返回；否则，从最接近的节点开始查找，直到找到目标键值对或遍历所有可选节点。
	 */
	bool get(uint64_t key, uint64_t &value)
	{
		// 初始化变量，表示是否找到目标键值对
		bool found = false;
		// 在本地数据库中查找键值对
		auto iter = _db->find(key);
		if (iter != _db->end())
		{
			value = iter->second;
			return true;
		}
		// 创建一个集合，用于记录已经访问过的节点的唯一标识
		set<uint64_t> nodes_;
		// 开始循环查找键值对
		while (true)
		{
			// 下一个节点信息
			Node next_node;
			// 查找离目标键最近的节点列表
			deque<Node> nodes = closeNodes();
			// 从节点列表中选择下一个节点，如果没有可选节点则退出循环
			if (pickNode(next_node, nodes, nodes_) == false)
			{
				break;
			}
			// 创建 gRPC 通道，连接到下一个节点的地址
			auto channel = grpc::CreateChannel(next_node.address(), grpc::InsecureChannelCredentials());
			std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
			ClientContext context;
			// 创建 IDKey 请求消息，用于发起查找值操作
			IDKey request;
			request.set_idkey((char *)(&key), sizeof(uint64_t));
			request.mutable_node()->CopyFrom(local_node);
			// 创建 KV_Node_Wrapper 响应消息，用于接收远程节点的响应
			KV_Node_Wrapper response;
			// 调用 find_value RPC 方法，发起查找值操作，并获取状态
			Status status = stub->find_value(&context, request, &response);
			// 检查是否找到目标键值对
			found = response.mode_kv();
			if (found)
			{
				// 从响应中获取响应节点信息和键值对的值，并更新本地节点信息
				Node resp_node = response.resp_node();
				std::string val = response.kv().value();
				memcpy(&value, val.c_str(), sizeof(uint64_t));
				freshNode(resp_node);
				break;
			}
			else
			{
				// 如果未找到目标键值对，则更新已访问节点的信息
				for (const auto &node : response.nodes())
				{
					freshNode(node);
				}
			}
			nodes_.insert(next_node.id());
		}
		// 返回是否找到目标键值对
		return found;
	}

	/*
	 * void put(uint64_t key, uint64_t value)
	 * 这个函数的主要目的是在接收到存储键值对的请求后，找到离键最近的节点，然后将键值对存储在该节点或本地数据库中。
	 */
	void put(uint64_t key, uint64_t value)
	{
		// 初始化目标节点为本地节点
		Node target_node = local_node;
		// 计算目标节点与键之间的距离
		uint64_t target_dis = id_distance(local_nodeId, key);
#ifdef DHASH_DEBUG
		// 打印节点表的调试信息
		printNodeTable();
#endif
		// 遍历所有存储桶，以查找距离键最近的节点
		for (uint64_t i = 0; i < num_buckets; i++)
		{
			// 锁定当前存储桶，防止并发访问
			lock->lock(i);
			// 遍历当前存储桶中的所有节点
			for (auto node : *(nodetable[i]))
			{
				// 计算当前节点与键之间的距离
				uint64_t dis = id_distance(node.id(), key);
				// 如果当前节点更接近键，更新目标节点和距离
				if (dis < target_dis)
				{
					target_dis = dis;
					target_node = node;
				}
			}
			// 解锁当前存储桶
			lock->unlock(i);
		}
		// 如果目标节点是本地节点，则将键值对存储在本地数据库
		if (local_nodeId == target_node.id())
		{
			_db->insert_or_assign(key, value);
		}
		else
		{
			// 创建 gRPC 通道，连接到目标节点的地址
			auto channel = grpc::CreateChannel(target_node.address(), grpc::InsecureChannelCredentials());
			std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
			ClientContext context;
			// 创建 KeyValue 请求消息，包含键值对信息
			KeyValue request;
			request.mutable_node()->CopyFrom(local_node);
			request.set_key((char *)(&key), sizeof(uint64_t));
			request.set_value((char *)(&value), sizeof(uint64_t));
			// 创建 IDKey 响应消息，用于接收远程节点的响应
			IDKey response;
			// 调用 store RPC 方法，发起存储键值对操作，并获取状态
			Status status = stub->store(&context, request, &response);
			// 更新本地节点信息
			freshNode(response.node());
		}
#ifdef DHASH_DEBUG
		// 打印节点表的调试信息
		printNodeTable();
#endif
	}

	/*
	 * void exit()
	 * 这个函数的主要目的是在本地节点准备退出时，通知其他节点，告知它们本地节点即将离开。
	 * 函数通过迭代遍历所有存储桶中的节点，创建 gRPC 通道，并调用 exit RPC 方法，以通知每个节点本地节点的退出。
	 */
	void exit()
	{
		// 创建 IDKey 请求消息，用于通知其他节点本地节点即将退出
		IDKey request, response;
		request.set_idkey((char *)(&local_nodeId), sizeof(uint64_t));
		request.mutable_node()->CopyFrom(local_node);
		// 遍历所有存储桶，以通知每个存储桶中的节点本地节点即将退出
		for (uint64_t i = 0; i < num_buckets; i++)
		{
			// 锁定当前存储桶，防止并发访问
			lock->lock(i);
			// 遍历当前存储桶中的所有节点
			for (auto node : *(nodetable[i]))
			{
				// 创建 gRPC 通道，连接到当前节点的地址
				auto channel = grpc::CreateChannel(node.address(), grpc::InsecureChannelCredentials());
				std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
				ClientContext context;
				// 调用 exit RPC 方法，通知当前节点本地节点即将退出
				stub->exit(&context, request, &response);
			}
			// 解锁当前存储桶
			lock->unlock(i);
		}
	}

	// not sure whether need to be implemented
	bool find_node(uint64_t nodeId)
	{
		return false;
	}

	uint64_t nodeId()
	{
		return local_nodeId;
	}

private:
	/*
	 * void freshNode(const Node node)
	 * 此方法用于维护节点表中的节点信息，通过计算节点之间的异或距离并将其插入到适当的位置，
	 * 以确保节点表包含距离本地节点最近的节点。
	 */
	void freshNode(const Node node)
	{
		// 获取目标节点的ID
		uint64_t target_id = node.id();
		// 如果目标节点ID与本地节点ID相同，直接返回，无需更新
		if (target_id == local_nodeId)
		{
			return;
		}
		// 计算目标节点与本地节点的异或距离
		uint64_t dis = id_distance(target_id, local_nodeId);
		// 计算异或距离对应的位数
		uint64_t k_dis = k_id_distance(dis);
		uint64_t i;
		// 获取互斥锁，锁定对应的位数
		lock->lock(k_dis);
		// 获取当前节点表中的节点数量
		uint64_t size = nodetable[k_dis]->size();
		// 查找目标节点是否已存在于节点表中
		for (i = 0; i < size; i++)
		{
			if ((*nodetable[k_dis])[i].id() == target_id)
			{
				break;
			}
		}
		// 如果目标节点已存在，将其从节点表中删除
		if (i < size)
		{
			nodetable[k_dis]->erase(nodetable[k_dis]->begin() + i);
		}
		// 将目标节点插入到节点表的开头
		nodetable[k_dis]->push_front(node);
		// 获取更新后的节点表大小
		size = nodetable[k_dis]->size();
		// 如果节点表超过了设定的最大节点数（k_closest），将多余的节点从末尾删除
		for (i = size - 1; i >= k_closest; i--)
		{
			nodetable[k_dis]->pop_back();
		}
		// 解锁互斥锁
		lock->unlock(k_dis);
	}

	deque<Node> closeNodes()
	{
		deque<Node> nodes;
		uint64_t num_nodes = 0;
		for (uint64_t i = 0; i < num_buckets; i++)
		{
			lock->lock(i);
			for (auto node : *(nodetable[i]))
			{
				nodes.push_back(node);
				num_nodes = num_nodes + 1;
				if (num_nodes >= k_closest)
				{
					lock->unlock(i);
					return nodes;
				}
			}
			lock->unlock(i);
		}
		return nodes;
	}

	bool pickNode(Node &node, deque<Node> waitq, set<uint64_t> visited)
	{
		for (Node node_ : waitq)
		{
			auto iter = visited.find(node_.id());
			if (iter == visited.end())
			{
				node = node_;
				return true;
			}
		}
		return false;
	}

	deque<Node> findCloseById(uint64_t target_id)
	{
		deque<Node> nodes;
		deque<std::pair<uint64_t, Node>> sorted;
		for (uint64_t i = 0; i < num_buckets; i++)
		{
			lock->lock(i);
			for (auto node : *(nodetable[i]))
			{
				uint64_t dis = id_distance(node.id(), target_id);
				sorted.push_back(std::make_pair(dis, node));
			}
			lock->unlock(i);
		}
		std::sort(sorted.begin(), sorted.end(),
				  [](std::pair<uint64_t, Node> const &a,
					 std::pair<uint64_t, Node> const &b)
				  {
					  return a.first < b.first;
				  });
		for (uint64_t i = 0; i < sorted.size() && i < k_closest; i++)
		{
			nodes.push_back(sorted[i].second);
		}
		return nodes;
	}

	void removeById(uint64_t target_id)
	{
		uint64_t dis = id_distance(local_nodeId, target_id);
		uint64_t k_dis = k_id_distance(dis);
		uint64_t i;
		lock->lock(k_dis);
		uint64_t size = nodetable[k_dis]->size();
		for (i = 0; i < size; i++)
		{
			if ((*nodetable[k_dis])[i].id() == target_id)
			{
				break;
			}
		}
		if (i < size)
		{
			nodetable[k_dis]->erase(nodetable[k_dis]->begin() + i);
		}
		lock->unlock(k_dis);
	}

	void printNodeTable()
	{
		printf("=========================================\n");
		for (uint64_t i = 0; i < num_buckets; i++)
		{
			std::cout << i << " ";
			for (auto node : *(nodetable[i]))
			{
				std::cout << node.id() << ":" << node.address() << ", ";
			}
			std::cout << std::endl;
		}
		printf("=========================================\n");
	}
};

#endif /* INCLUDE_NODEKADIMPL_HPP_ */
