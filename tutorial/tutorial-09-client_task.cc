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

#include <stdio.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFOperator.h>
#include "echo_pb.srpc.h"
#include "workflow/WFFacilities.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

int main()
{
	Example::SRPCClient client("127.0.0.1", 1412);

	EchoRequest req;
	req.set_message("Hello, sogou rpc!");
	req.set_name("1412");

	auto *rpc_task = client.create_Echo_task([](EchoResponse *resp,
												RPCContext *ctx)
	{
		if (ctx->success())
			printf("%s\n", resp->DebugString().c_str());
		else
			printf("status[%d] error[%d] errmsg:%s\n",
					ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());

		wait_group.done();
	});

	auto *http_task = WFTaskFactory::create_http_task("https://www.sogou.com",
													  0, 0,
													  [](WFHttpTask *task)
	{
		if (task->get_state() == WFT_STATE_SUCCESS)
		{
			std::string body;
			const void *data;
			size_t len;

			task->get_resp()->get_parsed_body(&data, &len);
			body.assign((const char *)data, len);
			printf("%s\n\n", body.c_str());
		}
		else
			printf("Http request fail\n\n");
	});

	rpc_task->serialize_input(&req);
	(*http_task > rpc_task).start();
	rpc_task->log({{"event", "info"}, {"message", "rpc client task log."}});
	wait_group.wait();

	return 0;
}

