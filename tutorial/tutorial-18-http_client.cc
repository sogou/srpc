/*
  Copyright (c) 2023 sogou, Inc.

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
#include "srpc/http_client.h"
#include "srpc/rpc_types.h"
#include "srpc/rpc_metrics_filter.h"
#include "srpc/rpc_trace_filter.h"

using namespace srpc;

#define REDIRECT_MAX	5
#define RETRY_MAX		2

srpc::RPCTraceDefault trace_log;
static WFFacilities::WaitGroup wait_group(1); 

void callback(WFHttpTask *task)
{
	int state = task->get_state();
	int error = task->get_error();
	fprintf(stderr, "callback. state = %d error = %d\n", state, error);

	if (state == WFT_STATE_SUCCESS) // print server response body
	{
		const void *body;
		size_t body_len;

		task->get_resp()->get_parsed_body(&body, &body_len);
		fwrite(body, 1, body_len, stdout);
		fflush(stdout);
		fprintf(stderr, "\nfinish print body. body_len = %zu\n", body_len);
	}
}

int main()
{
	srpc::HttpClient client;
	client.add_filter(&trace_log);

	WFHttpTask *task = client.create_http_task("http://127.0.0.1:1412",
											   REDIRECT_MAX,
											   RETRY_MAX,
											   callback);
	task->start();
	wait_group.wait();

	return 0;
}

