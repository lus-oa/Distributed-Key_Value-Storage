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
	using Status = grpc::Status;
	using ServerContext = grpc::ServerContext;
	using ClientContext = grpc::ClientContext;

	using Nodes = google::protobuf::RepeatedPtrField<Node>;

	std::string local_address = "";
	uint64_t local_nodeId = 0;
	uint64_t k_closest = 2;
	uint64_t num_buckets = 4;
	Node local_node;

	// NodeTable, need design.
	deque<Node> **nodetable;
	vector<Node> *sbuff_, *cbuff_;
	map<uint64_t, uint64_t> *_db;

	Lock *lock;

public:
	NodeKadImpl(std::string address, uint64_t id, uint64_t k = 2)
	{
		local_address = address;
		local_nodeId = id;
		local_node.set_address(local_address);
		local_node.set_id(local_nodeId);
		k_closest = k;
		num_buckets = pow(2, k);
		lock = new Lock(num_buckets);
		nodetable = new deque<Node> *[num_buckets];
		for (int i = 0; i < num_buckets; i++)
		{
			nodetable[i] = new deque<Node>();
		}
		_db = new map<uint64_t, uint64_t>();
		sbuff_ = new vector<Node>();
		cbuff_ = new vector<Node>();
	}

	Status find_node(ServerContext *context, const IDKey *request,
					 NodeList *response)
	{
		printf("find_node 1 %lu\n", local_nodeId);
		uint64_t target_id = str2u64(request->idkey());
		deque<Node> nodes = findCloseById(target_id);
		response->mutable_resp_node()->CopyFrom(local_node);
		printf("find_node 2 %lu\n", local_nodeId);
		for (Node node_ : nodes)
		{
			Node *node = response->add_nodes();
			node->set_address(node_.address());
			node->set_id(node_.id());
		}
		printf("find_node 3 %lu\n", local_nodeId);
		freshNode(request->node());
		return Status::OK;
	}

	Status find_value(ServerContext *context, const IDKey *request,
					  KV_Node_Wrapper *response)
	{
		uint64_t key = str2u64(request->idkey());
		auto iter = _db->find(key);
		response->mutable_resp_node()->CopyFrom(local_node);
		if (iter != _db->end())
		{
			uint64_t value = iter->second;
			response->set_mode_kv(true);
			KeyValue kv;
			kv.mutable_node()->CopyFrom(local_node);
			kv.set_key((char *)(&key), sizeof(uint64_t));
			kv.set_value((char *)(&value), sizeof(uint64_t));
			response->mutable_kv()->CopyFrom(kv);
		}
		else
		{
			response->set_mode_kv(false);
			deque<Node> nodes = findCloseById(key);
			for (Node node_ : nodes)
			{
				Node *node = response->add_nodes();
				node->set_address(node_.address());
				node->set_id(node_.id());
			}
		}
		freshNode(request->node());
		return Status::OK;
	}

	Status store(ServerContext *context, const KeyValue *request,
				 IDKey *response)
	{
		uint64_t key = str2u64(request->key());
		uint64_t value = str2u64(request->value());
		(*_db)[key] = value;
		freshNode(request->node());
		response->set_idkey((char *)(&local_nodeId), sizeof(uint64_t));
		response->mutable_node()->CopyFrom(local_node);
		return Status::OK;
	}

	Status exit(ServerContext *context, const IDKey *request, IDKey *response)
	{
		uint64_t target_id = str2u64(request->idkey());
		removeById(target_id);
		response->set_idkey((char *)(&local_nodeId), sizeof(uint64_t));
		response->mutable_node()->CopyFrom(local_node);
		return Status::OK;
	}

	void join(std::string address)
	{
		auto channel = grpc::CreateChannel(address,
										   grpc::InsecureChannelCredentials());
		std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
		ClientContext context;

		IDKey request;
		request.set_idkey((char *)(&local_nodeId), sizeof(uint64_t));
		request.mutable_node()->CopyFrom(local_node);
		NodeList response;
		Status status = stub->find_node(&context, request, &response);
		Node resp_node = response.resp_node();
		Nodes remote_nodes = response.nodes();
#ifdef DHASH_DEBUG
		printNodeTable();
#endif
		freshNode(resp_node);
		for (const auto &node : remote_nodes)
		{
			freshNode(node);
		}
#ifdef DHASH_DEBUG
		printNodeTable();
#endif
	}

	bool get(uint64_t key, uint64_t &value)
	{
		//		std::cout << "get 1" << std::endl;
		bool found = false;
		auto iter = _db->find(key);
		if (iter != _db->end())
		{
			value = iter->second;
			return true;
		}
		//		std::cout << "get 2" << std::endl;
		set<uint64_t> nodes_; // visited nodes id
		while (true)
		{
			Node next_node;
			deque<Node> nodes = closeNodes();
			if (pickNode(next_node, nodes, nodes_) == false)
			{
				break;
			}
			//			std::cout << "get 3" << std::endl;
			auto channel = grpc::CreateChannel(next_node.address(),
											   grpc::InsecureChannelCredentials());
			std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
			ClientContext context;

			IDKey request;
			request.set_idkey((char *)(&key), sizeof(uint64_t));
			request.mutable_node()->CopyFrom(local_node);
			KV_Node_Wrapper response;
			Status status = stub->find_value(&context, request, &response);
			//			std::cout << "get 4" << std::endl;
			found = response.mode_kv();
			if (found)
			{
				Node resp_node = response.resp_node();
				std::string val = response.kv().value();
				memcpy(&value, val.c_str(), sizeof(uint64_t));
				//				std::cout << "get 5 " << key << "->" << value << std::endl;
				freshNode(resp_node);
				break;
			}
			else
			{
				//				std::cout << "get 6 " << key << " direct " << std::endl;
				for (const auto &node : response.nodes())
				{
					freshNode(node);
				}
			}
			nodes_.insert(next_node.id());
		}
		return found;
	}

	void put(uint64_t key, uint64_t value)
	{
		Node target_node = local_node;
		uint64_t target_dis = id_distance(local_nodeId, key);
#ifdef DHASH_DEBUG
		printNodeTable();
#endif
		for (uint64_t i = 0; i < num_buckets; i++)
		{
			lock->lock(i);
			for (auto node : *(nodetable[i]))
			{
				uint64_t dis = id_distance(node.id(), key);
				if (dis < target_dis)
				{
					target_dis = dis;
					target_node = node;
				}
			}
			lock->unlock(i);
		}
		if (local_nodeId == target_node.id())
		{
			_db->insert_or_assign(key, value);
		}
		else
		{
			auto channel = grpc::CreateChannel(target_node.address(),
											   grpc::InsecureChannelCredentials());
			std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
			ClientContext context;

			KeyValue request;
			request.mutable_node()->CopyFrom(local_node);
			request.set_key((char *)(&key), sizeof(uint64_t));
			request.set_value((char *)(&value), sizeof(uint64_t));
			IDKey response;
			Status status = stub->store(&context, request, &response);
			freshNode(response.node());
		}
#ifdef DHASH_DEBUG
		printNodeTable();
#endif
	}

	void exit()
	{
		IDKey request, response;
		request.set_idkey((char *)(&local_nodeId), sizeof(uint64_t));
		request.mutable_node()->CopyFrom(local_node);
		//		std::cout << "exit 1 " << local_nodeId << std::endl;
		for (uint64_t i = 0; i < num_buckets; i++)
		{
			lock->lock(i);
			//			std::cout << "exit 2 " << local_nodeId << std::endl;
			for (auto node : *(nodetable[i]))
			{
				auto channel = grpc::CreateChannel(node.address(),
												   grpc::InsecureChannelCredentials());
				std::unique_ptr<KadImpl::Stub> stub = KadImpl::NewStub(channel);
				ClientContext context;

				stub->exit(&context, request, &response);
			}
			lock->unlock(i);
		}
		//		std::cout << "exit 3 " << local_nodeId << std::endl;
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
	void freshNode(const Node node)
	{
		uint64_t target_id = node.id();
		if (target_id == local_nodeId)
		{
			return;
		}
		uint64_t dis = id_distance(target_id, local_nodeId);
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
		// insert
		nodetable[k_dis]->push_front(node);
		size = nodetable[k_dis]->size();
		for (i = size - 1; i >= k_closest; i--)
		{
			nodetable[k_dis]->pop_back();
		}
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
