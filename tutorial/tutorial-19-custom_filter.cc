/*
  Copyright (c) 2024 sogou, Inc.

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

#include <signal.h>
#include "echo_pb.srpc.h"
#include "workflow/WFFacilities.h"
#include "srpc/rpc_types.h"

#include "srpc/rpc_module.h"
#include "srpc/rpc_basic.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

// Filter is available for both server and client.
// Please choose the function to implement the logic at the corresponding time.
class MyFilter : public RPCFilter
{
public:
	MyFilter() : RPCFilter(RPCModuleTypeCustom)
	{
	}

	bool server_begin(SubTask *task, RPCModuleData& data) override
	{	
		auto iter = data.find("my_auth_key");
		if (iter != data.end() && iter->second.compare("my_auth_value") == 0)
		{
			fprintf(stderr, "[FILTER] auth success : %s\n", iter->second.c_str());
			return true;
		}

		fprintf(stderr, "[FILTER] auth failed : %s\n",
				iter == data.end() ? "No auth" : iter->second.c_str());
		return false;
	}
};

class ExampleServiceImpl : public Example::Service
{
public:
	void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
	{
		resp->set_message("Hi back");
		fprintf(stderr, "[SERVER] Echo() get req: %s\n", req->message().c_str());
	}
};

static void sig_handler(int signo)
{
	wait_group.done();
}

void send_client_task()
{
	Example::SRPCClient client("127.0.0.1", 1412);

	auto callback = [](EchoResponse *resp, RPCContext *ctx)
	{
		if (ctx->success())
			fprintf(stderr, "[CLIENT] callback success\n");
		else
			fprintf(stderr, "[CLIENT] callback status[%d] error[%d] errmsg : %s\n",
					ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
	};

	EchoRequest req;
	req.set_name("Tutorial 19");

	// send one task to test the success case
	auto *task = client.create_Echo_task(callback);
	req.set_message("For success case");
	task->serialize_input(&req);
	task->add_baggage("my_auth_key", "my_auth_value");
	task->start();

	// send another task to test the failure case
	task = client.create_Echo_task(callback);
	req.set_message("For failure case");
	task->serialize_input(&req);
	task->add_baggage("my_auth_key", "randomxxx");
	task->start();
}

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	// 1. prepare server
	SRPCServer server;
	ExampleServiceImpl impl;
	server.add_service(&impl);

	// 2. add filter into server
	MyFilter my;
	server.add_filter(&my);

	// 3. run server
	if (server.start(1412) == 0)
	{
		fprintf(stderr, "[SERVER] Server with filter is running on 1412\n");

		// 4. send client task to test
		send_client_task();

		wait_group.wait();
		server.stop();
	}
	else
		perror("[SERVER] server start");

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}

