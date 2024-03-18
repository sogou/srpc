/*
  Copyright (c) 2021 sogou, Inc.

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
#include "helloworld.srpc.h"
#include "workflow/WFFacilities.h"
#include "srpc/rpc_types.h"

using namespace srpc;
using namespace trpc::test::helloworld;

static WFFacilities::WaitGroup wait_group(1);

class GreeterServiceImpl : public Greeter::Service
{
public:
	void SayHello(HelloRequest *req, HelloReply *resp, RPCContext *ctx) override
	{
		ctx->set_compress_type(RPCCompressGzip);
		resp->set_msg("This is SRPC framework TRPC protocol. Hello back.");

		printf("Server SayHello()\nget_req:\n%s\nset_resp:\n%s\n",
		req->DebugString().c_str(), resp->DebugString().c_str());
	}

	void SayHi(HelloRequest *req, HelloReply *resp, RPCContext *ctx) override
	{
		resp->set_msg("This is SRPC framework TRPC protocol. Hi back.");

		printf("Server SayHi()\nget_req:\n%s\nset_resp:\n%s\n",
		req->DebugString().c_str(), resp->DebugString().c_str());
	}
};

static void sig_handler(int signo)
{
	wait_group.done();
}

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	TRPCServer server;
	GreeterServiceImpl impl;

	server.add_service(&impl);

	if (server.start(1412) == 0)
	{
		printf("SRPC framework TRPC server is running on 1412\n"
			   "Try ./trpc_pb_client to send requests.\n\n");
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}

