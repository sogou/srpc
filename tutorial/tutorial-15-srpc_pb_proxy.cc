/*
  Copyright (c) 2020 sogou, Inc.

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
#include "srpc/rpc_span_policies.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

template<class CLIENT>
class ExampleProxyServiceImpl : public Example::Service
{
public:
	ExampleProxyServiceImpl(CLIENT *client)
	{
		this->client = client;
	}

private:
	CLIENT *client;

public:
	void Echo(EchoRequest *request, EchoResponse *response, RPCContext *context) override
	{
		auto *task = this->client->create_Echo_task([response](EchoResponse *resp,
															   RPCContext *ctx) {
			if (ctx->success())
				*response = std::move(*resp);
		});
		task->serialize_input(request);
		context->get_series()->push_back(task);
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

	SRPCServer server;
	Example::SRPCClient client("127.0.0.1", 1412);
	ExampleProxyServiceImpl<Example::SRPCClient> impl(&client);

	server.add_service(&impl);

	if (server.start(61412) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

