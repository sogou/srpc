/*
  Copyright (c) 2023 Sogou, Inc.

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

#include "http_module.h"

namespace srpc
{

bool HttpTraceModule::client_begin(SubTask *task, RPCModuleData& data)
{
	TraceModule<HttpServerTask, HttpClientTask>::client_begin(task, data);

	auto *client_task = static_cast<HttpClientTask *>(task);
	auto *req = client_task->get_req();

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	data[SRPC_HTTP_METHOD] = req->get_method();

	return true;
}

bool HttpTraceModule::client_end(SubTask *task, RPCModuleData& data)
{
	TraceModule<HttpServerTask, HttpClientTask>::client_end(task, data);

//	std::string ip;
//	unsigned short port;
	auto *client_task = static_cast<HttpClientTask *>(task);
	auto *resp = client_task->get_resp();

	if (client_task->get_state() == WFT_STATE_SUCCESS)
		data[SRPC_HTTP_STATUS_CODE] = resp->get_status_code();
	else
		data[SRPC_ERROR] = client_task->get_error();

	return true;
}

bool HttpTraceModule::server_begin(SubTask *task, RPCModuleData& data)
{
	TraceModule<HttpServerTask, HttpClientTask>::server_begin(task, data);

	auto *server_task = static_cast<HttpServerTask *>(task);
	auto *req = server_task->get_req();

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	data[SRPC_HTTP_METHOD] = req->get_method();
	data[SRPC_HTTP_TARGET] = req->get_request_uri();
//	data[SRPC_HTTP_HOST_NAME] = server_task->hostname;
//	data[SRPC_HTTP_HOST_PORT] = server_task->port;

	char addrstr[128];
	struct sockaddr_storage addr;
	socklen_t l = sizeof addr;
	unsigned short port = 0;

	server_task->get_peer_addr((struct sockaddr *)&addr, &l);
	if (addr.ss_family == AF_INET)
	{
		data[SRPC_HTTP_SOCK_FAMILY] = "inet";

		struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
		inet_ntop(AF_INET, &sin->sin_addr, addrstr, 128);
		port = ntohs(sin->sin_port);
	}
	else if (addr.ss_family == AF_INET6)
	{
		data[SRPC_HTTP_SOCK_FAMILY] = "inet6";

		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
		inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, 128);
		port = ntohs(sin6->sin6_port);
	}
	// else : Unknown

	data[SRPC_HTTP_SOCK_ADDR] = addrstr;
	data[SRPC_HTTP_SOCK_PORT] = std::to_string(port);

/*
	protocol::HttpHeaderCursor req_cursor(req);
	while (req_cursor.next(name, value))
	{
		if (name.casecmp("X-Forwarded-For") == 0)
			data[SRPC_HTTP_CLIENT_IP] = value;
	}
*/
	return true;
}

bool HttpTraceModule::server_end(SubTask *task, RPCModuleData& data)
{
	TraceModule<HttpServerTask, HttpClientTask>::server_end(task, data);

/*
	auto *server_task = static_cast<HttpServerTask *>(task);
	auto *resp = server_task->get_resp();

	data[SRPC_STATE] = std::to_string(resp->get_status_code());
	data[SRPC_ERROR] = std::to_string(resp->get_error());
*/
	return true;
}

bool HttpMetricsModule::client_begin(SubTask *task, RPCModuleData& data)
{
	MetricsModule<HttpServerTask, HttpClientTask>::client_begin(task, data);

/*
	auto *client_task = static_cast<HttpClientTask *>(task);
	auto *req = client_task->get_req();

	data[OTLP_SERVICE_NAME] = req->get_service_name();
	data[OTLP_METHOD_NAME] = req->get_method_name();
*/

	return true;
}

bool HttpMetricsModule::server_begin(SubTask *task, RPCModuleData& data)
{
	MetricsModule<HttpServerTask, HttpClientTask>::server_begin(task, data);

/*
	auto *server_task = static_cast<HttpServerTask *>(task);
	auto *req = server_task->get_req();

	data[OTLP_SERVICE_NAME] = req->get_service_name();
	data[OTLP_METHOD_NAME] = req->get_method_name();
*/

	return true;
}

} // end namespace srpc

