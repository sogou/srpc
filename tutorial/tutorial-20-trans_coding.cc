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

	SRPCHttpServer server;
	ExampleServiceImpl impl;

	std::string trans_coding = R"(
{
	"/test/echo"          : "Echo",
	"/another/for/echo"   : "Echo",
	"/no/method/for/test" : "Test"
}
)";

	server.add_service(&impl, trans_coding);

	if (server.start(1412) == 0)
	{
		printf("SRPC framework SRPC HTTP Protobuf server is running on 1412\n"
			   "Try CURL to send HTTP requests:\n"
			   "Example:\n"
			   "    curl localhost:1412/test/echo "
               "-H 'Content-Type: application/json' "
			   "-d '{message:\"trans_coding path\", name:\"tutorial-20\"}'\n"
			   "    curl localhost:1412/Example/Echo "
               "-H 'Content-Type: application/json' "
			   "-d '{message:\"origin path\", name:\"tutorial-20\"}'\n"
			   "\n");
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}

