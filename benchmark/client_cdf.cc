#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <thread>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include "benchmark_pb.srpc.h"
#include "benchmark_thrift.srpc.h"

using namespace srpc;

#define TEST_SECOND		20
#define GET_CURRENT_NS	std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()

std::atomic<int> query_count(0);
std::atomic<int> slow_count(0);
std::atomic<int> success_count(0);
std::atomic<int> error_count(0);
//std::atomic<int64_t> latency_sum(0);

std::vector<std::vector<int64_t>> latency_lists;

volatile bool stop_flag = false;
int PARALLEL_NUMBER;
std::string request_msg;
int QPS;

template<class CLIENT>
static void do_echo_pb(CLIENT *client, int idx)
{
	std::mutex mutex;
	auto& latency_list = latency_lists[idx];
	FixLengthPBMsg req;
	req.set_msg(request_msg);
	int usleep_gap = 1000000 / QPS * PARALLEL_NUMBER;

	while (!stop_flag)
	{
		int64_t ns_st = GET_CURRENT_NS;

		if (++query_count % 100 > 0)
		{
			client->echo_pb(&req, [client, ns_st, &latency_list, &mutex](EmptyPBMsg *response, RPCContext *ctx) {
				if (ctx->success())
				{
					//printf("%s\n", ctx->get_remote_ip().c_str());
					++success_count;
					//latency_sum += GET_CURRENT_NS - ns_st;
					mutex.lock();
					latency_list.emplace_back(GET_CURRENT_NS - ns_st);
					mutex.unlock();
				}
				else
				{
					printf("status[%d] error[%d] errmsg:%s\n", ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
					++error_count;
				}

				//printf("echo done. seq_id=%d\n", ctx->get_task_seq());
			});
		}
		else
		{
			client->slow_pb(&req, [client, ns_st](EmptyPBMsg *response, RPCContext *ctx) {
				slow_count++;
				if (!ctx->success())
					printf("status[%d] error[%d] errmsg:%s\n", ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
			});
		}

		std::this_thread::sleep_for(std::chrono::microseconds(usleep_gap));
	}
}

template<class CLIENT>
static void do_echo_thrift(CLIENT *client, int idx)
{
	std::mutex mutex;
	auto& latency_list = latency_lists[idx];
	BenchmarkThrift::echo_thriftRequest req;
	req.msg = request_msg;
	BenchmarkThrift::slow_thriftRequest slow_req;
	slow_req.msg = request_msg;
	int usleep_gap = 1000000 / QPS * PARALLEL_NUMBER;

	while (!stop_flag)
	{
		int64_t ns_st = GET_CURRENT_NS;

		if (++query_count % 100 > 0)
		{
			client->echo_thrift(&req, [client, ns_st, &latency_list, &mutex](BenchmarkThrift::echo_thriftResponse *response, RPCContext *ctx) {
				if (ctx->success())
				{
					//printf("%s\n", ctx->get_remote_ip().c_str());
					++success_count;
					//latency_sum += GET_CURRENT_NS - ns_st;
					mutex.lock();
					latency_list.emplace_back(GET_CURRENT_NS - ns_st);
					mutex.unlock();
				}
				else
				{
					printf("status[%d] error[%d] errmsg:%s \n", ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
					++error_count;
				}

				//printf("echo done. seq_id=%d\n", ctx->get_task_seq());
			});
		}
		else
		{
			client->slow_thrift(&slow_req, [client, ns_st](BenchmarkThrift::slow_thriftResponse *response, RPCContext *ctx) {
				slow_count++;
				if (!ctx->success())
					printf("status[%d] error[%d] errmsg:%s\n", ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
			});
		}

		std::this_thread::sleep_for(std::chrono::microseconds(usleep_gap));
	}
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if (argc != 8)
	{
		fprintf(stderr, "Usage: %s <IP> <PORT> <srpc|brpc|thrift> <pb|thrift> <PARALLEL_NUMBER> <REQUEST_BYTES> <QPS>\n", argv[0]);
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
	QPS = atoi(argv[7]);

	request_msg.resize(REQUEST_BYTES, 'r');
	//for (int i = 0; i < REQUEST_BYTES; i++)
	//	request_msg[i] = (unsigned char)(rand() % 256);

	latency_lists.resize(PARALLEL_NUMBER);
	std::vector<std::thread *> th;
	int64_t start = GET_CURRENT_MS();

	if (server_type == "srpc")
	{
		if (idl_type == "pb")
		{
			auto *client = new BenchmarkPB::SRPCClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				th.push_back(new std::thread(do_echo_pb<BenchmarkPB::SRPCClient>, client, i));
		}
		else if (idl_type == "thrift")
		{
			auto *client = new BenchmarkThrift::SRPCClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				th.push_back(new std::thread(do_echo_thrift<BenchmarkThrift::SRPCClient>, client, i));
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
				th.push_back(new std::thread(do_echo_pb<BenchmarkPB::BRPCClient>, client, i));
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
				th.push_back(new std::thread(do_echo_thrift<BenchmarkThrift::ThriftClient>, client, i));
			else
				abort();
		}
	}
	else if (server_type == "srpc_http")
	{
		if (idl_type == "pb")
		{
			auto * client = new BenchmarkPB::SRPCHttpClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				th.push_back(new std::thread(do_echo_pb<BenchmarkPB::SRPCHttpClient>, client, i));
		}
		else if (idl_type == "thrift")
		{
			auto *client = new BenchmarkThrift::SRPCHttpClient(&client_params);

			for (int i = 0; i < PARALLEL_NUMBER; i++)
				th.push_back(new std::thread(do_echo_thrift<BenchmarkThrift::SRPCHttpClient>, client, i));
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
				th.push_back(new std::thread(do_echo_thrift<BenchmarkThrift::ThriftHttpClient>, client, i));
			else
				abort();
		}
	}
	else
		abort();

	std::this_thread::sleep_for(std::chrono::seconds(TEST_SECOND));
	stop_flag = true;
	for (auto *t : th)
	{
		t->join();
		delete t;
	}

	int64_t end = GET_CURRENT_MS();
	int tot = query_count - slow_count;
	int s = success_count;
	int e = error_count;
	int64_t l = 0;//latency_sum;
	std::vector<int64_t> all_lc;
	for (const auto& list : latency_lists)
	{
		for (auto v : list)
		{
			//fprintf(stderr, "%lld\n", (long long int)v);
			l += v;
		}

		all_lc.insert(all_lc.end(), list.begin(), list.end());
	}

	sort(all_lc.begin(), all_lc.end());
	for (double r = 0.950; r <= 0.999; r += 0.001)
	{
		double d = r * all_lc.size();
		int idx = (int)(d + 1.0e-8);
		if (fabs(d - int(d)) > 1.0e-8)
			idx++;

		printf("%.3lf %lld\n", r, (long long int)all_lc[idx - 1]/1000);
	}

	//printf("%.3lf %lld\n", 1.0, (long long int)all_lc[all_lc.size() - 1]/1000);

	fprintf(stderr, "\nquery\t%d\ttimes, %d success, %d error.\n", tot, s, e);
	fprintf(stderr, "total\t%.3lf\tseconds\n", (end - start) / 1000.0);
	fprintf(stderr, "qps=%.0lf\n", tot * 1000.0 / (end - start));
	fprintf(stderr, "latency=%.0lfus\n", s > 0 ? l * 1.0 / s / 1000 : 0);

	std::this_thread::sleep_for(std::chrono::seconds(1));
	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

