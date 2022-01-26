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
#include "echo_thrift.srpc.h"
#include "workflow/WFFacilities.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

class ExampleServiceImpl : public Example::Service
{
public:
	void Echo(EchoResult& _return, const std::string& message,
			  const std::string& name) override
	{
		_return.message = "Hi back, " + name;

		printf("Server Echo()\nreq_message:\n%s\nresp_message:\n%s\n",
				message.c_str(), _return.message.c_str());
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

	if (server.start(1412) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

