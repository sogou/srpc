#include <stdio.h>
#include <signal.h>
#include "benchmark_pb.srpc.h"
#include "benchmark_thrift.srpc.h"

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace srpc;

class BenchmarkPBServiceImpl : public BenchmarkPB::Service
{
public:
	void echo_pb(FixLengthPBMsg *request, EmptyPBMsg *response, RPCContext *ctx) override { }
	void slow_pb(FixLengthPBMsg *request, EmptyPBMsg *response, RPCContext *ctx) override {
		auto *task = WFTaskFactory::create_timer_task(15000, nullptr);

		ctx->get_series()->push_back(task);
	}
};

class BenchmarkThriftServiceImpl : public BenchmarkThrift::Service
{
public:
	void echo_thrift(BenchmarkThrift::echo_thriftRequest *request, BenchmarkThrift::echo_thriftResponse *response, RPCContext *ctx) override { }
	void slow_thrift(BenchmarkThrift::slow_thriftRequest *request, BenchmarkThrift::slow_thriftResponse *response, RPCContext *ctx) override {
		auto *task = WFTaskFactory::create_timer_task(15000, nullptr);

		ctx->get_series()->push_back(task);
	}
};

static void sig_handler(int signo)
{
}

static void run_srpc_server(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SRPCServer server(&params);

	BenchmarkPBServiceImpl pb_impl;
	BenchmarkThriftServiceImpl thrift_impl;
	server.add_service(&pb_impl);
	server.add_service(&thrift_impl);

	if (server.start(port) == 0)
	{
#ifndef _WIN32
		pause();
#else
		getchar();
#endif
		server.stop();
	}
	else
		perror("server start");
}

template<class SERVER>
static void run_pb_server(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SERVER server(&params);

	BenchmarkPBServiceImpl pb_impl;
	server.add_service(&pb_impl);

	if (server.start(port) == 0)
	{
#ifndef _WIN32
		pause();
#else
		getchar();
#endif
		server.stop();
	}
	else
		perror("server start");
}

template<class SERVER>
static void run_thrift_server(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SERVER server(&params);

	BenchmarkThriftServiceImpl thrift_impl;
	server.add_service(&thrift_impl);

	if (server.start(port) == 0)
	{
#ifndef _WIN32
		pause();
#else
		getchar();
#endif
		server.stop();
	}
	else
		perror("server start");
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <PORT> <srpc|brpc|thrift>\n", argv[0]);
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
		run_srpc_server(port);
	else if (server_type == "brpc")
		run_pb_server<BRPCServer>(port);
	else if (server_type == "thrift")
		run_thrift_server<ThriftServer>(port);
	else if (server_type == "srpc_http")
		run_pb_server<SRPCHttpServer>(port);
	else if (server_type == "thrift_http")
		run_thrift_server<ThriftHttpServer>(port);
	else
		abort();

	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

