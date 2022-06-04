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

		this->filter->histogram("echo_request_size")->observe(req->ByteSizeLong());

		for (size_t i = 1; i <= 10; i++)
			this->filter->summary("echo_test_quantiles")->observe(0.01 * i);
	}

	void set_filter(RPCMetricsPull *filter) { this->filter = filter; }

private:
	RPCMetricsPull *filter;
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
	RPCMetricsPull filter;

	filter.init(8080); /* export port for prometheus */
	filter.create_histogram("echo_request_size", "Echo request size",
							{1, 10, 100});
	filter.create_summary("echo_test_quantiles", "Test quantile",
						  {{0.5, 0.05}, {0.9, 0.01}});
	impl.set_filter(&filter);

	server.add_filter(&filter);
	server.add_service(&impl);

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

