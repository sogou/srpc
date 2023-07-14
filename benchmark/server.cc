#include <stdio.h>
#include <signal.h>
#include "benchmark_pb.srpc.h"
#include "benchmark_thrift.srpc.h"
#include "workflow/WFFacilities.h"
#ifdef _WIN32
#include "workflow/PlatformSocket.h"
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


using namespace srpc;

std::atomic<int> query_count(0); // per_second
std::atomic<long long> last_timestamp(0L);
//volatile bool stop_flag = false;
int max_qps = 0;
long long total_count = 0;
WFFacilities::WaitGroup wait_group(1);

inline void collect_qps()
{
	int64_t ms_timestamp = GET_CURRENT_MS();
	++query_count;
	if (ms_timestamp / 1000 > last_timestamp)
	{
		last_timestamp = ms_timestamp / 1000;

		int count = query_count;
		query_count = 0;
		total_count += count;
		if (count > max_qps)
			max_qps = count;
		long long ts = ms_timestamp;
		fprintf(stdout, "TIMESTAMP(ms) = %llu QPS = %d\n", ts, count);
	}
}

class BenchmarkPBServiceImpl : public BenchmarkPB::Service
{
public:
	void echo_pb(FixLengthPBMsg *request, EmptyPBMsg *response,
				 RPCContext *ctx) override
	{
		collect_qps();
	}

	void slow_pb(FixLengthPBMsg *request, EmptyPBMsg *response,
				 RPCContext *ctx) override
	{
		auto *task = WFTaskFactory::create_timer_task(15000, nullptr);
		ctx->get_series()->push_back(task);
	}
};

class BenchmarkThriftServiceImpl : public BenchmarkThrift::Service
{
public:
	void echo_thrift(BenchmarkThrift::echo_thriftRequest *request,
					 BenchmarkThrift::echo_thriftResponse *response,
					 RPCContext *ctx) override
	{
		collect_qps();
	}

	void slow_thrift(BenchmarkThrift::slow_thriftRequest *request,
					 BenchmarkThrift::slow_thriftResponse *response,
					 RPCContext *ctx) override
	{
		auto *task = WFTaskFactory::create_timer_task(15000, nullptr);
		ctx->get_series()->push_back(task);
	}
};

static void sig_handler(int signo)
{
	wait_group.done();
}

static inline int create_bind_socket(unsigned short port)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd >= 0)
	{
		struct sockaddr_in sin = { };
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(sockfd, (struct sockaddr *)&sin, sizeof sin) >= 0)
			return sockfd;

		close(sockfd);
	}

	return -1;
}

static void run_srpc_server(unsigned short port, int proc_num)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SRPCServer server(&params);

	BenchmarkPBServiceImpl pb_impl;
	BenchmarkThriftServiceImpl thrift_impl;
	server.add_service(&pb_impl);
	server.add_service(&thrift_impl);

	int sockfd = create_bind_socket(port);

	if (sockfd < 0)
	{
		perror("create socket");
		exit(1);
	}

	while ((proc_num /= 2) != 0)
		fork();

	if (server.serve(sockfd) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	close(sockfd);
}

template<class SERVER>
static void run_pb_server(unsigned short port, int proc_num)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SERVER server(&params);

	BenchmarkPBServiceImpl pb_impl;
	server.add_service(&pb_impl);

	int sockfd = create_bind_socket(port);

	if (sockfd < 0)
	{
		perror("create socket");
		exit(1);
	}

	while ((proc_num /= 2) != 0)
		fork();

	if (server.serve(sockfd) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	close(sockfd);
}

template<class SERVER>
static void run_thrift_server(unsigned short port, int proc_num)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SERVER server(&params);

	BenchmarkThriftServiceImpl thrift_impl;
	server.add_service(&thrift_impl);

	int sockfd = create_bind_socket(port);

	if (sockfd < 0)
	{
		perror("create socket");
		exit(1);
	}

	while ((proc_num /= 2) != 0)
		fork();

	if (server.serve(sockfd) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	close(sockfd);
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	int proc_num = 1;

	if (argc == 4)
	{
		proc_num = atoi(argv[3]);
		if (proc_num != 1 && proc_num != 2 && proc_num != 4 && proc_num != 8 && proc_num != 16)
		{
			fprintf(stderr, "Usage: %s <PORT> <srpc|brpc|thrift> [proc num (1/2/4/8/16)]\n", argv[0]);
			abort();
		}
	}
	else if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <PORT> <srpc|brpc|thrift> [proc num (1/2/4/8/16)]\n", argv[0]);
		abort();
	}

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	WFGlobalSettings my = GLOBAL_SETTINGS_DEFAULT;
	my.poller_threads = 16;
	my.handler_threads = 16;
	WORKFLOW_library_init(&my);

	unsigned short port = atoi(argv[1]);
	std::string server_type = argv[2];

	if (server_type == "srpc")
		run_srpc_server(port, proc_num);
	else if (server_type == "brpc")
		run_pb_server<BRPCServer>(port, proc_num);
	else if (server_type == "thrift")
		run_thrift_server<ThriftServer>(port, proc_num);
	else if (server_type == "srpc_http")
		run_pb_server<SRPCHttpServer>(port, proc_num);
	else if (server_type == "thrift_http")
		run_thrift_server<ThriftHttpServer>(port, proc_num);
	else
		abort();

	fprintf(stdout, "\nTotal query: %llu max QPS: %d\n", total_count, max_qps);
	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

