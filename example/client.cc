#include <string>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "echo.srpc.h"
#include "msg.srpc.h"

using namespace sogou;
using namespace example;

#define PARALLEL_NUM	20
#define GET_CURRENT_MS	std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
#define GET_CURRENT_NS	std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

std::atomic<int> query_count(0);
std::atomic<int> success_count(0);
std::atomic<int> error_count(0);
std::atomic<int64_t> latency_sum(0);
int64_t start, end;
volatile bool stop_flag = false;

EchoRequest g_echo_req;
ExampleThrift::MessageRequest g_msg_req;

template<class CLIENT>
static void do_echo(CLIENT *client)
{
	EchoRequest req;
	req.set_message("Hello, sogou rpc!");
	req.set_name("Jeff Dean");

	int64_t ns_st = GET_CURRENT_NS;
 
	client->Echo(&req, [client, ns_st](EchoResponse *response, RPCContext *ctx) {
		if (!stop_flag)
			do_echo<CLIENT>(client);

		++query_count;
		if (ctx->success())
		{
//			printf("%s\n", response->message().c_str());
//			printf("%s\n", ctx->get_remote_ip().c_str());
			++success_count;
			latency_sum += GET_CURRENT_NS - ns_st;
		}
		else
		{
			printf("status[%d] error[%d] errmsg:%s\n", ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
			++error_count;
		}

		//printf("echo done. seq_id=%d\n", ctx->get_task_seq());
	});
}

template<class CLIENT>
static void do_msg(CLIENT *client)
{
	ExampleThrift::MessageRequest req;
	req.message = "Hello, sogou rpc!";
	req.name = "Jeff Dean";

	int64_t ns_st = GET_CURRENT_NS;

	client->Message(&req, [client, ns_st](ExampleThrift::MessageResponse *response, RPCContext *ctx) {
		if (!stop_flag)
			do_msg<CLIENT>(client);

		++query_count;
		if (ctx->success())
		{
			//printf("%s\n", response->result.message.c_str());
			//printf("%s\n", ctx->get_remote_ip().c_str());
			++success_count;
			latency_sum += GET_CURRENT_NS - ns_st;
		}
		else
		{
			printf("status[%d] error[%d] errmsg:%s \n", ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
			++error_count;
		}

		//printf("echo done. seq_id=%d\n", ctx->get_task_seq());
	});
}

void sig_handler(int signo)
{
	stop_flag = true;
	end = GET_CURRENT_MS;
	int tot = query_count;
	int s = success_count;
	int e = error_count;
	int64_t l = latency_sum;

	fprintf(stderr, "\nquery\t%d\ttimes, %d success, %d error.\n", tot, s, e);
	fprintf(stderr, "total\t%.3lf\tseconds\n", (end - start) / 1000.0);
	fprintf(stderr, "qps=%.0lf\n", tot * 1000.0 / (end - start));
	fprintf(stderr, "latency=%.0lfus\n", s > 0 ? l * 1.0 / s / 1000 : 0);
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if (argc < 4 || argc > 5)
	{
		fprintf(stderr, "Usage: %s <srpc|brpc|thrift> <echo|msg> <IP> <PORT> or %s <srpc|brpc|thrift> <echo|msg> <URL>\n",
				argv[0], argv[0]);
		return 0;
	}

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	WFGlobalSettings setting = GLOBAL_SETTINGS_DEFAULT;
	setting.endpoint_params.max_connections = 1000;
	setting.poller_threads = 16;
	setting.handler_threads = 16;
	WORKFLOW_library_init(&setting);

	RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
	client_params.task_params.keep_alive_timeout = -1;
//	client_params.task_params.data_type = RPCDataJson;
//	client_params.task_params.compress_type = RPCCompressGzip;

//	UpstreamManager::upstream_create_weighted_random("echo_server", true);
//	UpstreamManager::upstream_add_server("echo_server", "sostest12.web.gd.ted:1412");
//	UpstreamManager::upstream_add_server("echo_server", "sostest11.web.gd.ted:1412");
//	UpstreamManager::upstream_add_server("echo_server", "sostest11.web.gd.ted:1411");

	std::string server_type = argv[1];
	std::string service_name = argv[2];

	if (argc == 4)
	{
//		client_params.url = "http://echo_server";//for upstream
		client_params.url = argv[3];
	} else {
		client_params.host = argv[3];
		client_params.port = atoi(argv[4]);
	}

	start = GET_CURRENT_MS;
	if (server_type == "srpc")
	{
		if (service_name == "echo")
		{
			ExamplePB::SRPCClient client(&client_params);

			for (int i = 0; i < PARALLEL_NUM; i++)
				do_echo(&client);
		}
		else if (service_name == "msg")
		{
			ExampleThrift::SRPCClient client(&client_params);

			for (int i = 0; i < PARALLEL_NUM; i++)
				do_msg(&client);
		}
		else
			abort();
	}
	else if (server_type == "brpc")
	{
		ExamplePB::BRPCClient client(&client_params);

		for (int i = 0; i < PARALLEL_NUM; i++)
		{
			if (service_name == "echo")
				do_echo(&client);
			else if (service_name == "msg")
				abort();
			else
				abort();
		}
	}
	else if (server_type == "thrift")
	{
		ExampleThrift::ThriftClient client(&client_params);

		for (int i = 0; i < PARALLEL_NUM; i++)
		{
			if (service_name == "echo")
				abort();
			else if (service_name == "msg")
				do_msg(&client);
			else
				abort();
		}
	}
	else if (server_type == "srpc_http")
	{
		if (service_name == "echo")
		{
			ExamplePB::SRPCHttpClient client(&client_params);

			for (int i = 0; i < PARALLEL_NUM; i++)
				do_echo(&client);
		}
		else if (service_name == "msg")
		{
			ExampleThrift::SRPCHttpClient client(&client_params);

			for (int i = 0; i < PARALLEL_NUM; i++)
				do_msg(&client);
		}
		else
			abort();
	}
	else if (server_type == "thrift_http")
	{
		ExampleThrift::ThriftHttpClient client(&client_params);

		for (int i = 0; i < PARALLEL_NUM; i++)
		{
			if (service_name == "echo")
				abort();
			else if (service_name == "msg")
				do_msg(&client);
			else
				abort();
		}
	}
	else
		abort();

	pause();
	sleep(2);
	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

