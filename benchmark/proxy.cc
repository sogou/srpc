#include <stdio.h>
#include <signal.h>
#include "benchmark_pb.srpc.h"
#include "benchmark_thrift.srpc.h"
#include "workflow/WFFacilities.h"

using namespace srpc;

std::atomic<int> query_count(0); // per_second
std::atomic<long long> last_timestamp(0L);
//volatile bool stop_flag = false;
int max_qps = 0;
long long total_count = 0;
std::string remote_host;
unsigned short remote_port;
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

template<class CLIENT>
class BenchmarkPBServiceImpl : public BenchmarkPB::Service
{
public:
	void echo_pb(FixLengthPBMsg *request, EmptyPBMsg *response,
				 RPCContext *ctx) override
	{
		auto *task = this->client->create_echo_pb_task(
			[](EmptyPBMsg *remote_resp, srpc::RPCContext *remote_ctx) {
				collect_qps();
			});
		task->user_data = response;
		task->serialize_input(request);
		ctx->get_series()->push_back(task);
	}

	void slow_pb(FixLengthPBMsg *request, EmptyPBMsg *response,
				 RPCContext *ctx) override
	{
		auto *task = WFTaskFactory::create_timer_task(15000, nullptr);
		ctx->get_series()->push_back(task);
	}

	CLIENT *client;
};

template<class CLIENT>
class BenchmarkThriftServiceImpl : public BenchmarkThrift::Service
{
public:
	void echo_thrift(BenchmarkThrift::echo_thriftRequest *request,
					 BenchmarkThrift::echo_thriftResponse *response,
					 RPCContext *ctx) override
	{
		auto *task = this->client->create_echo_thrift_task(
			[](BenchmarkThrift::echo_thriftResponse *remote_resp,
			   srpc::RPCContext *remote_ctx) {
					collect_qps();
		});
		task->user_data = response;
		task->serialize_input(request);
		ctx->get_series()->push_back(task);
	}

	void slow_thrift(BenchmarkThrift::slow_thriftRequest *request,
					 BenchmarkThrift::slow_thriftResponse *response,
					 RPCContext *ctx) override
	{
		auto *task = WFTaskFactory::create_timer_task(15000, nullptr);
		ctx->get_series()->push_back(task);
	}

	CLIENT *client;
};

static void sig_handler(int signo)
{
	wait_group.done();
}

template<template<class> class SERVICE, class CLIENT>
static void init_proxy_client(SERVICE<CLIENT>& service_impl)
{
	RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
	client_params.task_params.keep_alive_timeout = -1;
	client_params.host = remote_host;
	client_params.port = remote_port;
	service_impl.client = new CLIENT(&client_params);
}

static void run_srpc_proxy(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SRPCServer proxy_server(&params);

	BenchmarkPBServiceImpl<BenchmarkPB::SRPCClient> pb_impl;
	BenchmarkThriftServiceImpl<BenchmarkThrift::SRPCClient> thrift_impl;

	init_proxy_client<BenchmarkPBServiceImpl,
					  BenchmarkPB::SRPCClient>(pb_impl);
	init_proxy_client<BenchmarkThriftServiceImpl,
					  BenchmarkThrift::SRPCClient>(thrift_impl);

	proxy_server.add_service(&pb_impl);
	proxy_server.add_service(&thrift_impl);

	if (proxy_server.start(port) == 0)
	{
		wait_group.wait();
		proxy_server.stop();
	}
	else
		perror("server start");
}

template<class SERVER, class CLIENT>
static void run_pb_proxy(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SERVER server(&params);

	BenchmarkPBServiceImpl<CLIENT> pb_impl;
	init_proxy_client<BenchmarkPBServiceImpl, CLIENT>(pb_impl);
	server.add_service(&pb_impl);

	if (server.start(port) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");
}

template<class SERVER, class CLIENT>
static void run_thrift_proxy(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SERVER server(&params);

	BenchmarkThriftServiceImpl<CLIENT> thrift_impl;
	init_proxy_client<BenchmarkThriftServiceImpl, CLIENT>(thrift_impl);
	server.add_service(&thrift_impl);

	if (server.start(port) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if (argc != 5)
	{
		fprintf(stderr, "Usage: %s <PORT> <REMOTE_IP> <REMOTE_PORT>"
				" <srpc|brpc|thrift>\n", argv[0]);
		abort();
	}

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	WFGlobalSettings my = GLOBAL_SETTINGS_DEFAULT;
	my.poller_threads = 16;
	my.handler_threads = 16;
	WORKFLOW_library_init(&my);

	unsigned short port = atoi(argv[1]);
	remote_host = argv[2];
	remote_port = atoi(argv[3]);
	std::string server_type = argv[4];

	if (server_type == "srpc")
		run_srpc_proxy(port);
	else if (server_type == "brpc")
		run_pb_proxy<BRPCServer, BenchmarkPB::BRPCClient>(port);
	else if (server_type == "thrift")
		run_thrift_proxy<ThriftServer, BenchmarkThrift::ThriftClient>(port);
	else if (server_type == "srpc_http")
		run_pb_proxy<SRPCHttpServer, BenchmarkPB::SRPCHttpClient>(port);
	else if (server_type == "thrift_http")
		run_thrift_proxy<ThriftHttpServer, BenchmarkThrift::ThriftHttpClient>(port);
	else
		abort();

	fprintf(stdout, "\nTotal query: %llu max QPS: %d\n", total_count, max_qps);
	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

