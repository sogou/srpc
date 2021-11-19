#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <string>
#include <atomic>
#include <chrono>
#include "benchmark_pb.srpc.h"
#include "benchmark_thrift.srpc.h"

using namespace srpc;

#define TEST_SECOND		20
#define GET_CURRENT_NS	std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

std::atomic<int> query_count(0);
std::atomic<int> success_count(0);
std::atomic<int> error_count(0);
std::atomic<int64_t> latency_sum(0);

volatile bool stop_flag = false;
int PARALLEL_NUMBER;
std::string request_msg;

template<class CLIENT>
static void do_echo_pb(CLIENT *client)
{
	FixLengthPBMsg req;
	req.set_msg(request_msg);

	int64_t ns_st = GET_CURRENT_NS;
	++query_count;
 
	client->echo_pb(&req, [client, ns_st](EmptyPBMsg *response, RPCContext *ctx) {
		if (ctx->success())
		{
			//printf("%s\n", ctx->get_remote_ip().c_str());
			latency_sum += GET_CURRENT_NS - ns_st;
			++success_count;
		}
		else
		{
			printf("status[%d] error[%d] errmsg:%s\n",
				   ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
			++error_count;
		}

		if (!stop_flag)
			do_echo_pb<CLIENT>(client);

		//printf("echo done. seq_id=%d\n", ctx->get_task_seq());
	});
}

template<class CLIENT>
static void do_echo_thrift(CLIENT *client)
{
	BenchmarkThrift::echo_thriftRequest req;
	req.msg = request_msg;

	int64_t ns_st = GET_CURRENT_NS;
	++query_count;

	client->echo_thrift(&req, [client, ns_st](BenchmarkThrift::echo_thriftResponse *response, RPCContext *ctx) {
		if (ctx->success())
		{
			//printf("%s\n", ctx->get_remote_ip().c_str());
			latency_sum += GET_CURRENT_NS - ns_st;
			++success_count;
		}
		else
		{
			printf("status[%d] error[%d] errmsg:%s \n",
				   ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
			++error_count;
		}

		if (!stop_flag)
			do_echo_thrift<CLIENT>(client);

		//printf("echo done. seq_id=%d\n", ctx->get_task_seq());
	});
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if (argc != 7)
	{
		fprintf(stderr, "Usage: %s <IP> <PORT> <srpc|brpc|thrift> <pb|thrift> <PARALLEL_NUMBER> <REQUEST_BYTES>\n", argv[0]);
		abort();
	}

	WFGlobalSettings setting = GLOBAL_SETTINGS_DEFAULT;
	setting.endpoint_params.max_connections = 2048;
	setting.poller_threads = 16;
	setting.handler_threads = 16;
	WORKFLOW_library_init(&setting);

	RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
	client_params.task_params.keep_alive_timeout = -1;

	client_params.host = argv[1];
	client_params.port = atoi(argv[2]);

	std::string server_type = argv[3];
	std::string idl_type = argv[4];

	PARALLEL_NUMBER = atoi(argv[5]);
	int REQUEST_BYTES = atoi(argv[6]);

	request_msg.resize(REQUEST_BYTES, 'r');

	//for (int i = 0; i < REQUEST_BYTES; i++)
	//	request_msg[i] = (unsigned char)(rand() % 256);

	int64_t start = GET_CURRENT_MS();

	if (server_type == "srpc")
	{
		if (idl_type == "pb")
		{
			auto *client = new BenchmarkPB::SRPCClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				do_echo_pb(client);
		}
		else if (idl_type == "thrift")
		{
			auto *client = new BenchmarkThrift::SRPCClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				do_echo_thrift(client);
		}
		else
			abort();
	}
	else if (server_type == "brpc")
	{
		auto *client = new BenchmarkPB::BRPCClient(&client_params);

		for (int i = 0; i < PARALLEL_NUMBER; i++)
		{
			if (idl_type == "pb")
				do_echo_pb(client);
			else if (idl_type == "thrift")
				abort();
			else
				abort();
		}
	}
	else if (server_type == "thrift")
	{
		auto *client = new BenchmarkThrift::ThriftClient(&client_params);

		for (int i = 0; i < PARALLEL_NUMBER; i++)
		{
			if (idl_type == "pb")
				abort();
			else if (idl_type == "thrift")
				do_echo_thrift(client);
			else
				abort();
		}
	}
	else if (server_type == "srpc_http")
	{
		if (idl_type == "pb")
		{
			auto *client = new BenchmarkPB::SRPCHttpClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				do_echo_pb(client);
		}
		else if (idl_type == "thrift")
		{
			auto *client = new BenchmarkThrift::SRPCHttpClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				do_echo_thrift(client);
		}
		else
			abort();
	}
	else if (server_type == "thrift_http")
	{
		auto *client = new BenchmarkThrift::ThriftHttpClient(&client_params);

		for (int i = 0; i < PARALLEL_NUMBER; i++)
		{
			if (idl_type == "pb")
				abort();
			else if (idl_type == "thrift")
				do_echo_thrift(client);
			else
				abort();
		}
	}
	else
		abort();

	std::this_thread::sleep_for(std::chrono::seconds(TEST_SECOND));
	stop_flag = true;

	int64_t end = GET_CURRENT_MS();
	int tot = query_count;
	int s = success_count;
	int e = error_count;
	int64_t l = latency_sum;

	fprintf(stderr, "\nquery\t%d\ttimes, %d success, %d error.\n", tot, s, e);
	fprintf(stderr, "total\t%.3lf\tseconds\n", (end - start) / 1000.0);
	fprintf(stderr, "qps=%.0lf\n", tot * 1000.0 / (end - start));
	fprintf(stderr, "latency=%.0lfus\n", s > 0 ? l * 1.0 / s / 1000 : 0);

	std::this_thread::sleep_for(std::chrono::seconds(1));
	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

