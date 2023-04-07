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

#include <signal.h>
#include "workflow/WFFacilities.h"
#include "srpc/http_server.h"
#include "srpc/rpc_types.h"
#include "srpc/rpc_metrics_filter.h"
#include "srpc/rpc_trace_filter.h"

static WFFacilities::WaitGroup wait_group(1);

srpc::RPCMetricsPull  exporter;
srpc::RPCTraceDefault trace_log;

static void sig_handler(int signo)
{
	wait_group.done();
}

void process(WFHttpTask *task)
{
    fprintf(stderr, "http server get request_uri: %s\n",
            task->get_req()->get_request_uri());

    task->get_resp()->append_output_body("<html>Hello from server!</html>");
}

int main()
{
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	srpc::HttpServer server(process);

	exporter.init(8080); /* export port for prometheus */
	exporter.create_histogram("echo_request_size", "Echo request size",
							{1, 10, 100});
	exporter.create_summary("echo_test_quantiles", "Test quantile",
						  {{0.5, 0.05}, {0.9, 0.01}});

	server.add_filter(&trace_log);	
	server.add_filter(&exporter);

	if (server.start(1412) == 0)
	{
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");

	exporter.deinit();
	return 0;
}

