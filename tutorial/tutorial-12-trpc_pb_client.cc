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

#include <stdio.h>
#include "workflow/WFFacilities.h"
#include "helloworld.srpc.h"
#include "srpc/rpc_types.h"

using namespace srpc;
using namespace trpc::test::helloworld;

static WFFacilities::WaitGroup wait_group(1);

int main()
{
	Greeter::TRPCClient client("127.0.0.1", 1412);

	//async
	HelloRequest req;
	req.set_msg("Hello, trpc server. This is srpc framework trpc client!");

	client.SayHello(&req, [](HelloReply *resp, RPCContext *ctx) {
		if (ctx->success())
			printf("%s\n", resp->DebugString().c_str());
		else
			printf("status[%d] error[%d] errmsg:%s\n",
					ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
		wait_group.done();
	});

	//sync
	HelloRequest sync_req;
	HelloReply sync_resp;
	RPCSyncContext sync_ctx;

	sync_req.set_msg("Hi, trpc server. This is srpc framework trpc client!");
	client.SayHi(&sync_req, &sync_resp, &sync_ctx);

	if (sync_ctx.success)
		printf("%s\n", sync_resp.DebugString().c_str());
	else
		printf("status[%d] error[%d] errmsg:%s\n",
				sync_ctx.status_code, sync_ctx.error, sync_ctx.errmsg.c_str());

	wait_group.wait();
	return 0;
}

