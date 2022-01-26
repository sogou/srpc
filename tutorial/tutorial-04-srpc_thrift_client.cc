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
#include "echo_thrift.srpc.h"

using namespace srpc;

int main()
{
	Example::SRPCClient client("127.0.0.1", 1412);

	//sync
	EchoResult sync_res;

	client.Echo(sync_res, "Hello, sogou rpc!", "1412");

	if (client.thrift_last_sync_success())
		printf("%s\n", sync_res.message.c_str());
	else
	{
		const auto& sync_ctx = client.thrift_last_sync_ctx();

		printf("status[%d] error[%d] errmsg:%s\n",
				sync_ctx.status_code, sync_ctx.error, sync_ctx.errmsg.c_str());
	}

	//async
	Example::EchoRequest req;
	req.message = "Hello, sogou rpc!";
	req.name = "1412";

	client.Echo(&req, [](Example::EchoResponse *resp, RPCContext *ctx) {
		if (ctx->success())
			printf("%s\n", resp->result.message.c_str());
		else
			printf("status[%d] error[%d] errmsg:%s\n",
					ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
	});

	//sync
	Example::EchoRequest sync_req;
	Example::EchoResponse sync_resp;
	RPCSyncContext sync_ctx;

	sync_req.message = "Hello, sogou rpc!";
	sync_req.name = "Sync";

	client.Echo(&sync_req, &sync_resp, &sync_ctx);

	if (sync_ctx.success)
		printf("%s\n", sync_resp.result.message.c_str());
	else
		printf("status[%d] error[%d] errmsg:%s\n",
				sync_ctx.status_code, sync_ctx.error, sync_ctx.errmsg.c_str());

	return 0;
}

