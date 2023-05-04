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

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace srpc
{

bool HttpTraceModule::client_begin(SubTask *task, RPCModuleData& data)
{
	TraceModule::client_begin(task, data);

	auto *client_task = static_cast<HttpClientTask *>(task);
	TraceModule::client_begin_request(client_task->get_req(), data);

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;

	data[SRPC_HTTP_PEER_NAME] = client_task->get_uri_host();
	data[SRPC_HTTP_PEER_PORT] = client_task->get_uri_port();
	data[SRPC_HTTP_SCHEME] = client_task->get_uri_scheme();
	// TODO: data[SRPC_HTTP_CLIENT_URL] = client_task->get_url();

	return true;
}

bool HttpTraceModule::client_end(SubTask *task, RPCModuleData& data)
{
	TraceModule::client_end(task, data);

	auto *client_task = static_cast<HttpClientTask *>(task);

	data[WF_TASK_STATE] = std::to_string(client_task->get_state());
	if (client_task->get_state() != WFT_STATE_SUCCESS)
	{
		data[WF_TASK_ERROR] = std::to_string(client_task->get_error());
		if (client_task->get_error() == ETIMEDOUT)
		{
			data[SRPC_TIMEOUT_REASON] =
				std::to_string(client_task->get_timeout_reason());
		}

		return true;
	}

	TraceModule::client_end_response(client_task->get_resp(), data);

	data[SRPC_HTTP_RESEND_COUNT] = std::to_string(client_task->get_retry_times());

	char addrstr[128];
	struct sockaddr_storage addr;
	socklen_t l = sizeof addr;
	unsigned short port = 0;

	client_task->get_peer_addr((struct sockaddr *)&addr, &l);
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
	else
		memset(addrstr, 0, sizeof (addrstr));

	data[SRPC_HTTP_SOCK_ADDR] = addrstr;
	data[SRPC_HTTP_SOCK_PORT] = std::to_string(port);

	return true;
}

bool HttpTraceModule::server_begin(SubTask *task, RPCModuleData& data)
{
	TraceModule::server_begin(task, data);

	auto *server_task = static_cast<HttpServerTask *>(task);
	TraceModule::server_begin_request(server_task->get_req(), data);

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	data[SRPC_HTTP_SCHEME] = server_task->is_ssl() ? "https" : "http";
	data[SRPC_HTTP_HOST_PORT] = std::to_string(server_task->listen_port());

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

	return true;
}

bool HttpTraceModule::server_end(SubTask *task, RPCModuleData& data)
{
	TraceModule::server_end(task, data);
	auto *server_task = static_cast<HttpServerTask *>(task);

	data[SRPC_STATE] = std::to_string(server_task->get_state());
	if (server_task->get_state() != WFT_STATE_SUCCESS)
	{
		data[SRPC_ERROR] = std::to_string(server_task->get_error());
		if (server_task->get_error() == ETIMEDOUT)
		{
			data[SRPC_TIMEOUT_REASON] =
				std::to_string(server_task->get_timeout_reason());
		}

		return true;
	}

	TraceModule::server_end_response(server_task->get_resp(), data);

	return true;
}

bool HttpMetricsModule::client_begin(SubTask *task, RPCModuleData& data)
{
	MetricsModule::client_begin(task, data);
	// TODO:
	return true;
}

bool HttpMetricsModule::server_begin(SubTask *task, RPCModuleData& data)
{
	MetricsModule::server_begin(task, data);
	// TODO:
	return true;
}

} // end namespace srpc

