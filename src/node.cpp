/*
 * node.cpp
 */

#include <pthread.h>
#include <unistd.h>

#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>

#include "nodeKadImpl.hpp"

char ip_port[20] = "127.0.0.1:6900";
pthread_mutex_t exit_lock_ = PTHREAD_MUTEX_INITIALIZER;

struct para
{
	char address[20];
	uint64_t id;
	bool client;
};

pthread_barrier_t barrier;

void *run_server(void *para);
void *run_client(void *para);

void *run_server(void *para)
{
	// 解析参数结构体
	struct para *p = (struct para *)para;
	uint64_t id = p->id;		 // 获取节点的ID
	grpc::ServerBuilder builder; // 创建 gRPC 服务器构建器

	// 将传入的地址转换为字符串类型
	std::string str(p->address);
	// 将地址添加到 gRPC 服务器构建器并使用不安全的服务器凭据
	builder.AddListeningPort(str, grpc::InsecureServerCredentials());

	// 创建分布式哈希存储节点对象
	NodeKadImpl *node = new NodeKadImpl(str, id);
	// 注册节点服务到 gRPC 服务器
	builder.RegisterService(node);

	// 创建并启动 gRPC 服务器
	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	// 输出服务器的启动信息
	std::cout << "start " << str << " " << id << std::endl;

	// 如果节点是客户端
	if (p->client)
	{
		// 将节点加入到分布式哈希存储网络
		node->join("127.0.0.1:6900");
	}

	// 使用线程屏障等待其他线程就绪
	pthread_barrier_wait(&barrier);
	std::cout << "join done" << std::endl; // 输出加入网络完成的信息

	// 运行客户端代码
	run_client((void *)node);

	// 如果节点是客户端
	if (p->client)
	{
		pthread_mutex_lock(&exit_lock_);					  // 锁住互斥锁
		std::cout << id << " prepare to leave " << std::endl; // 输出节点准备离开的信息
		// 从分布式哈希存储网络中移除节点
		node->exit();
		std::cout << id << " exit" << std::endl; // 输出节点已经离开的信息
		sleep(1);								 // 休眠1秒，确保其他节点有足够的时间感知节点的离开
		pthread_mutex_unlock(&exit_lock_);		 // 解锁互斥锁
	}

	// 使用线程屏障等待其他线程完成
	pthread_barrier_wait(&barrier);
	printf("done\n"); // 输出完成的信息

	// 返回指向节点对象的指针
	return (void *)node;
}

void *run_client(void *para)
{
	// 每个客户端要插入的键值对数量
	uint64_t num_kv = 10000;
	NodeKadImpl *node = (NodeKadImpl *)para; // 获取分布式哈希存储节点对象
	uint64_t id = node->nodeId();			 // 获取节点的ID

	// 插入键值对到分布式哈希存储
	for (uint64_t i = 0; i < num_kv; i++)
	{
		uint64_t key = i * 2 + id + 1;
		node->put(key, key + 1);
		// 输出插入键值对的信息
	}

	// 使用线程屏障等待其他线程完成插入操作
	pthread_barrier_wait(&barrier);
	std::cout << "put done" << std::endl; // 输出插入完成的信息

	// 查询插入的键值对
	for (uint64_t i = 0; i < num_kv; i++)
	{
		uint64_t key = i * 2 + id + 2;
		uint64_t ret = 0;
		// 查询键值对，并将结果存储在 ret 中
		if (node->get(key, ret) && ret != (key + 1))
		{
			printf("error\n"); // 如果查询到的值不正确，输出错误信息
		}
		// 输出查询键值对的信息
	}

	// 使用线程屏障等待其他线程完成查询操作
	pthread_barrier_wait(&barrier);
	std::cout << "get done" << std::endl; // 输出查询完成的信息

	return NULL; // 返回空指针
}

int main(int argc, char **argv)
{
	char *address = ip_port;
	if (argc == 2)
	{
		address = argv[1];
	}
	int max_server = 4;
	int num_server = 4;
	pthread_barrier_init(&barrier, NULL, num_server);
	struct para p[max_server] = {{"127.0.0.1:6900", 2, false}, {"127.0.0.1:6901", 3, true}, {"127.0.0.1:6902", 5, true}, {"127.0.0.1:6903", 7, true}};

	pthread_t p_server[num_server];
	void *server[num_server];
	for (int i = 0; i < num_server; i++)
	{
		pthread_create(&p_server[i], NULL, run_server, (void *)&p[i]);
	}
	for (int i = 0; i < num_server; i++)
	{
		pthread_join(p_server[i], &server[i]);
	}
	return 0;
}
