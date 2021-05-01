/*
  Copyright (c) 2020 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <gtest/gtest.h>
#include <workflow/WFOperator.h>
#include "test_pb.srpc.h"
#include "test_thrift.srpc.h"

using namespace srpc;
using namespace unit;

class ForceShutdown
{
public:
	~ForceShutdown() { google::protobuf::ShutdownProtobufLibrary(); }
} g_holder;

class TestPBServiceImpl : public TestPB::Service
{
public:
	void Add(AddRequest *request, AddResponse *response, RPCContext *ctx) override
	{
		response->set_c(request->a() + request->b());
	}

	void Substr(SubstrRequest *request, SubstrResponse *response, RPCContext *ctx) override
	{
		if (request->has_length())
			response->set_str(std::string(request->str(), request->idx(), request->length()));
		else
			response->set_str(std::string(request->str(), request->idx()));
	}
};

class TestThriftServiceImpl : public TestThrift::Service
{
public:
	int32_t add(const int32_t a, const int32_t b) override
	{
		return a + b;
	}

	void substr(std::string& _return, const std::string& str, const int32_t idx, const int32_t length) override
	{
		if (length < 0)
			_return = std::string(str, idx);
		else
			_return = std::string(str, idx, length);
	}
};

template<class SERVER, class CLIENT>
void test_pb(SERVER& server)
{
	std::mutex mutex;
	std::condition_variable cond;
	bool done = false;

	RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
	TestPBServiceImpl impl;

	server.add_service(&impl);
	EXPECT_TRUE(server.start("127.0.0.1", 9964) == 0) << "server start failed";

	client_params.host = "127.0.0.1";
	client_params.port = 9964;
	CLIENT client(&client_params);

	AddRequest req1;

	req1.set_a(123);
	req1.set_b(456);
	client.Add(&req1, [&](AddResponse *response, RPCContext *ctx) {
		EXPECT_EQ(ctx->success(), true);
		if (ctx->success())
		{
			EXPECT_EQ(response->c(), 123 + 456);

			SubstrRequest req2;

			req2.set_str("hello world!");
			req2.set_idx(6);
			client.Substr(&req2, [&](SubstrResponse *response, RPCContext *ctx) {
				EXPECT_EQ(ctx->success(), true);
				EXPECT_TRUE(response->str() == "world!");
				mutex.lock();
				done = true;
				mutex.unlock();
				cond.notify_one();
			});
		}
		else
		{
			mutex.lock();
			done = true;
			mutex.unlock();
			cond.notify_one();
		}
	});

	std::unique_lock<std::mutex> lock(mutex);
	while (!done)
		cond.wait(lock);

	lock.unlock();

	AddResponse resp1;
	RPCSyncContext ctx1;
	client.Add(&req1, &resp1, &ctx1);
	EXPECT_EQ(ctx1.success, true);
	EXPECT_EQ(resp1.c(), 123 + 456);

	auto fr = client.async_Add(&req1);
	auto res = fr.get();
	EXPECT_EQ(res.second.success, true);
	EXPECT_EQ(res.first.c(), 123 + 456);

	server.stop();
}

template<class SERVER, class CLIENT>
void test_thrift(SERVER& server)
{
	std::mutex mutex;
	std::condition_variable cond;
	bool done = false;

	RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
	TestThriftServiceImpl impl;

	server.add_service(&impl);
	EXPECT_TRUE(server.start("127.0.0.1", 9965) == 0) << "server start failed";

	client_params.host = "127.0.0.1";
	client_params.port = 9965;
	CLIENT client(&client_params);

	TestThrift::addRequest req1;

	req1.a = 123;
	req1.b = 456;
	client.add(&req1, [&](TestThrift::addResponse *response, RPCContext *ctx) {
		EXPECT_EQ(ctx->success(), true);
		if (ctx->success())
		{
			EXPECT_EQ(response->result, 123 + 456);

			TestThrift::substrRequest req2;

			req2.str = "hello world!";
			req2.idx = 6;
			req2.length = -1;
			client.substr(&req2, [&](TestThrift::substrResponse *response, RPCContext *ctx) {
				EXPECT_EQ(ctx->success(), true);
				EXPECT_TRUE(response->result == "world!");
				mutex.lock();
				done = true;
				mutex.unlock();
				cond.notify_one();
			});
		}
		else
		{
			mutex.lock();
			done = true;
			mutex.unlock();
			cond.notify_one();
		}
	});

	std::unique_lock<std::mutex> lock(mutex);
	while (!done)
		cond.wait(lock);

	lock.unlock();

	int32_t c = client.add(123, 456);
	EXPECT_EQ(client.thrift_last_sync_success(), true);
	EXPECT_EQ(c, 123 + 456);

	client.send_add(123, 456);
	c = client.recv_add();
	EXPECT_EQ(client.thrift_last_sync_success(), true);
	EXPECT_EQ(c, 123 + 456);

	server.stop();
}

TEST(SRPC, unittest)
{
	RPCServerParams server_params = RPC_SERVER_PARAMS_DEFAULT;
	SRPCServer server(&server_params);

	test_pb<SRPCServer, TestPB::SRPCClient>(server);
	test_thrift<SRPCServer, TestThrift::SRPCClient>(server);
}

TEST(SRPCHttp, unittest)
{
	RPCServerParams server_params = RPC_SERVER_PARAMS_DEFAULT;
	SRPCHttpServer server(&server_params);

	test_pb<SRPCHttpServer, TestPB::SRPCHttpClient>(server);
}

TEST(BRPC, unittest)
{
	RPCServerParams server_params = RPC_SERVER_PARAMS_DEFAULT;
	BRPCServer server(&server_params);

	test_pb<BRPCServer, TestPB::BRPCClient>(server);
}

TEST(Thrift, unittest)
{
	RPCServerParams server_params = RPC_SERVER_PARAMS_DEFAULT;
	ThriftServer server(&server_params);

	test_thrift<ThriftServer, TestThrift::ThriftClient>(server);
}

TEST(ThriftHttp, unittest)
{
	RPCServerParams server_params = RPC_SERVER_PARAMS_DEFAULT;
	ThriftHttpServer server(&server_params);

	test_thrift<ThriftHttpServer, TestThrift::ThriftHttpClient>(server);
}

TEST(SRPC_COMPRESS, unittest)
{
	std::mutex mutex;
	std::condition_variable cond;
	bool done = false;

	RPCServerParams server_params = RPC_SERVER_PARAMS_DEFAULT;
	RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
	SRPCServer server(&server_params);
	TestPBServiceImpl impl;

	server.add_service(&impl);
	EXPECT_TRUE(server.start("127.0.0.1", 9964) == 0) << "server start failed";

	client_params.host = "127.0.0.1";
	client_params.port = 9964;
	TestPB::SRPCClient client(&client_params);

	AddRequest req;

	req.set_a(123);
	req.set_b(456);
	auto&& cb = [](AddResponse *response, RPCContext *ctx) {
		EXPECT_EQ(ctx->get_status_code(), RPCStatusOK);
		EXPECT_EQ(ctx->success(), true);
		EXPECT_EQ(response->c(), 123 + 456);
	};
	auto *t1 = client.create_Add_task(cb);
	auto *t2 = client.create_Add_task(cb);
	auto *t3 = client.create_Add_task(cb);
	auto *t4 = client.create_Add_task(cb);

	t1->set_compress_type(RPCCompressSnappy);
	t2->set_compress_type(RPCCompressGzip);
	t3->set_compress_type(RPCCompressZlib);
	t4->set_compress_type(RPCCompressLz4);
	t1->serialize_input(&req);
	t2->serialize_input(&req);
	t3->serialize_input(&req);
	t4->serialize_input(&req);

	auto& par = *t1 * t2 * t3 * t4;

	par.set_callback([&](const ParallelWork *par) {
		mutex.lock();
		done = true;
		mutex.unlock();
		cond.notify_one();
	});
	par.start();

	std::unique_lock<std::mutex> lock(mutex);
	while (!done)
		cond.wait(lock);

	lock.unlock();
	server.stop();
}

