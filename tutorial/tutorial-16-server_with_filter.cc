/*
  Copyright (c) 2022 sogou, Inc.

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
#include "srpc/rpc_filter_metrics.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

class ExampleServiceImpl : public Example::Service
{
public:
	void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
	{
		resp->set_message("Hi back");

		printf("Server Echo()\nget_req:\n%s\nset_resp:\n%s\n",
				req->DebugString().c_str(), resp->DebugString().c_str());

		RPCVarFactory::gauge<int>("request_count")->increase();
		RPCVarFactory::counter<int>("request_method")->add({{"protocol", "srpc"},
															{"method", "Echo"}})->increase();
		RPCVarFactory::histogram<double>("request_latency")->observe(rand() % 11);
		RPCVarFactory::summary<int>("request_body_size")->observe(req->ByteSizeLong());
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
	ExampleServiceImpl impl;

	server.add_service(&impl);

	RPCMetricsPull filter;
	filter.init(8080); /* export port for prometheus */
	server.add_filter(&filter);

	filter.create_gauge<int>("request_count", "request count");
	filter.create_counter<int>("request_method", "request method info");
	filter.create_histogram<double>("request_latency", "request latency",
									{0.1, 1.0, 10.0});
	filter.create_summary<int>("request_body_size", "request size count",
							   {{0.5, 0.05}, {0.9, 0.01}});

	if (server.start(1412) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	filter.deinit();
	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}

