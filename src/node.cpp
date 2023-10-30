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
	struct para *p = (struct para *)para;
	uint64_t id = p->id;
	grpc::ServerBuilder builder;
	std::string str(p->address);
	builder.AddListeningPort(str, grpc::InsecureServerCredentials());
	NodeKadImpl *node = new NodeKadImpl(str, id);
	builder.RegisterService(node);
	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	std::cout << "start " << str << " " << id << std::endl;
	if (p->client)
	{
		node->join("127.0.0.1:6900");
	}
	pthread_barrier_wait(&barrier);
	std::cout << "join done" << std::endl;
	// barrier -
	run_client((void *)node);
	if (p->client)
	{
		pthread_mutex_lock(&exit_lock_);
		std::cout << id << " prepare to leave " << std::endl;
		node->exit();
		std::cout << id << " exit" << std::endl;
		sleep(1);
		pthread_mutex_unlock(&exit_lock_);
	}
	pthread_barrier_wait(&barrier);
	printf("done\n");
	return (void *)node;
}

void *run_client(void *para)
{
	uint64_t num_kv = 10000;
	NodeKadImpl *node = (NodeKadImpl *)para;
	uint64_t id = node->nodeId();
	for (uint64_t i = 0; i < num_kv; i++)
	{
		uint64_t key = i * 2 + id + 1;
		node->put(key, key + 1);
		//		std::cout << "put key " << key << std::endl;
	}
	pthread_barrier_wait(&barrier);
	std::cout << "put done" << std::endl;
	for (uint64_t i = 0; i < num_kv; i++)
	{
		uint64_t key = i * 2 + id + 2;
		uint64_t ret = 0;
		if (node->get(key, ret) && ret != (key + 1))
		{
			printf("error\n");
		}
		//		std::cout << "get key " << key << std::endl;
	}
	pthread_barrier_wait(&barrier);
	std::cout << "get done" << std::endl;
	return NULL;
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
