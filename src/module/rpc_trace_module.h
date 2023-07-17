/*
  Copyright (c) 2020 Sogou, Inc.

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

#ifndef __RPC_TRACE_MODULE_H__
#define __RPC_TRACE_MODULE_H__

#include <time.h>
#include <atomic>
#include <limits>
#include <stdio.h>
#include <string>
#include "workflow/WFTask.h"
#include "rpc_basic.h"
#include "rpc_module.h"
#include "rpc_context.h"

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <sys/types.h>
#include <arpa/inet.h>
#endif

namespace srpc
{

// follows the basic conventions of RPC part in opentracing
static constexpr char const *SRPC_COMPONENT			= "srpc.component";
static constexpr char const *SRPC_SPAN_ID			= "srpc.span_id";
static constexpr char const *SRPC_TRACE_ID			= "srpc.trace_id";
static constexpr char const *SRPC_PARENT_SPAN_ID	= "srpc.parent_span_id";

// span tags
static constexpr char const *SRPC_SPAN_KIND			= "srpc.span.kind";
static constexpr char const *SRPC_SPAN_KIND_CLIENT	= "srpc.client";
static constexpr char const *SRPC_SPAN_KIND_SERVER	= "srpc.server";
static constexpr char const *SRPC_STATE				= "rpc.state";
static constexpr char const *SRPC_ERROR				= "rpc.error";
static constexpr char const *WF_TASK_STATE			= "task.state";
static constexpr char const *WF_TASK_ERROR			= "task.error";
static constexpr char const *SRPC_REMOTE_IP			= "server.address";
static constexpr char const *SRPC_REMOTE_PORT		= "server.port";
static constexpr char const *SRPC_SAMPLING_PRIO		= "srpc.sampling.priority";

// for ext tags
static constexpr char const *SRPC_DATA_TYPE			= "srpc.data.type";
static constexpr char const *SRPC_COMPRESS_TYPE		= "srpc.compress.type";

// for http
static constexpr char const *SRPC_HTTP_SOCK_FAMILY	= "net.sock.family";
static constexpr char const *SRPC_HTTP_SOCK_ADDR	= "net.sock.peer.addr";
static constexpr char const *SRPC_HTTP_SOCK_PORT	= "net.sock.peer.port";
static constexpr char const *SRPC_HTTP_REQ_LEN		= "http.request_content_length";
static constexpr char const *SRPC_HTTP_RESP_LEN		= "http.response_content_length";

// for http client
static constexpr char const *SRPC_HTTP_CLIENT_URL	= "http.url";
static constexpr char const *SRPC_HTTP_PEER_NAME	= "net.peer.name";
static constexpr char const *SRPC_HTTP_PEER_PORT	= "net.peer.port";
static constexpr char const *SRPC_HTTP_RESEND_COUNT	= "http.resend_count";

// for http server
static constexpr char const *SRPC_HTTP_SCHEME		= "http.scheme";
static constexpr char const *SRPC_HTTP_HOST_NAME	= "net.host.name";
static constexpr char const *SRPC_HTTP_HOST_PORT	= "net.host.port";
// /users/12314/?q=ddds
static constexpr char const *SRPC_HTTP_TARGET		= "http.target";
// The IP address of the original client behind all proxies, from X-Forwarded-For
static constexpr char const *SRPC_HTTP_CLIENT_IP	= "http.client_ip";

// Basic TraceModule for generating general span data.
// Each kind of network task can derived its own TraceModule.

class TraceModule : public RPCModule
{
public:
	bool client_begin(SubTask *task, RPCModuleData& data) override;
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_begin(SubTask *task, RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;

protected:
	void client_begin_request(protocol::HttpRequest *req,
							  RPCModuleData& data) const;
	void client_end_response(protocol::HttpResponse *resp,
							 RPCModuleData& data) const;
	void server_begin_request(protocol::HttpRequest *req,
							  RPCModuleData& data) const;
	void server_end_response(protocol::HttpResponse *resp,
							 RPCModuleData& data) const;

	void client_begin_request(protocol::ProtocolMessage *req,
							  RPCModuleData& data) const
	{
	}
	void client_end_response(protocol::ProtocolMessage *resp,
							 RPCModuleData& data) const
	{
	}
	void server_begin_request(protocol::ProtocolMessage *req,
							  RPCModuleData& data) const
	{
	}
	void server_end_response(protocol::ProtocolMessage *resp,
							 RPCModuleData& data) const
	{
	}

public:
	TraceModule() : RPCModule(RPCModuleTypeTrace)
	{
	}
};

// Fill RPC related data in trace module

template<class SERVER_TASK, class CLIENT_TASK>
class RPCTraceModule : public TraceModule
{
public:
	bool client_begin(SubTask *task, RPCModuleData& data) override;
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_begin(SubTask *task, RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;

private:
	void client_basic_info(CLIENT_TASK *task, RPCModuleData& data);
	void server_basic_info(SERVER_TASK *task, RPCModuleData& data);
};

////////// impl

template<class STASK, class CTASK>
void RPCTraceModule<STASK, CTASK>::client_basic_info(CTASK *task,
													 RPCModuleData& data)
{
	auto *req = task->get_req();

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	data[OTLP_SERVICE_NAME] = req->get_service_name();
	if (req->get_method_name().length() > 0 &&
		req->get_method_name().at(0) == '/')
	{
		data[OTLP_METHOD_NAME] = req->get_method_name().substr(1,
										req->get_method_name().length() - 1);
	}
	else
		data[OTLP_METHOD_NAME] = req->get_method_name();

	data[SRPC_DATA_TYPE] = std::to_string(req->get_data_type());
	data[SRPC_COMPRESS_TYPE] = std::to_string(req->get_compress_type());
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::client_begin(SubTask *task,
												RPCModuleData& data)
{
	TraceModule::client_begin(task, data);

	auto *client_task = static_cast<CTASK *>(task);
	this->client_basic_info(client_task, data);

	auto *req = client_task->get_req();
	TraceModule::client_begin_request(req, data);

	return true;
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::client_end(SubTask *task,
											  RPCModuleData& data)
{
	TraceModule::client_end(task, data);

	std::string ip;
	unsigned short port;
	auto *client_task = static_cast<CTASK *>(task);
	auto *resp = client_task->get_resp();

	// failed to call client_begin
	if (data.find(SRPC_COMPONENT) == data.end())
		this->client_basic_info(client_task, data);

	data[SRPC_STATE] = std::to_string(resp->get_status_code());

	if (resp->get_status_code() != RPCStatusOK)
	{
		data[SRPC_ERROR] = std::to_string(resp->get_error());
		return true;
	}

	if (client_task->get_remote(ip, &port))
	{
		data[SRPC_REMOTE_IP] = std::move(ip);
		data[SRPC_REMOTE_PORT] = std::to_string(port);
	}

	TraceModule::client_end_response(resp, data);

	return true;
}

template<class STASK, class CTASK>
void RPCTraceModule<STASK, CTASK>::server_basic_info(STASK *task,
													 RPCModuleData& data)
{
	std::string ip;
	unsigned short port;
	auto *req = task->get_req();

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	data[OTLP_SERVICE_NAME] = req->get_service_name();
	if (req->get_method_name().length() > 0 &&
		req->get_method_name().at(0) == '/')
	{
		data[OTLP_METHOD_NAME] = req->get_method_name().substr(1,
										req->get_method_name().length() - 1);
	}
	else
		data[OTLP_METHOD_NAME] = req->get_method_name();

	data[SRPC_DATA_TYPE] = std::to_string(req->get_data_type());
	data[SRPC_COMPRESS_TYPE] = std::to_string(req->get_compress_type());

	if (task->get_remote(ip, &port))
	{
		data[SRPC_REMOTE_IP] = std::move(ip);
		data[SRPC_REMOTE_PORT] = std::to_string(port);
	}
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::server_begin(SubTask *task,
												RPCModuleData& data)
{
	TraceModule::server_begin(task, data);

	auto *server_task = static_cast<STASK *>(task);
	this->server_basic_info(server_task, data);

	auto *req = server_task->get_req();
	TraceModule::server_begin_request(req, data);

	return true;
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::server_end(SubTask *task,
											  RPCModuleData& data)
{
	TraceModule::server_end(task, data);

	auto *server_task = static_cast<STASK *>(task);
	auto *resp = server_task->get_resp();

	// failed to call server_begin
	if (data.find(SRPC_COMPONENT) == data.end())
		this->server_basic_info(server_task, data);

	data[SRPC_STATE] = std::to_string(resp->get_status_code());

	if (resp->get_status_code() != RPCStatusOK)
	{
		data[SRPC_ERROR] = std::to_string(resp->get_error());
		return true;
	}

	TraceModule::client_end_response(resp, data);

	return true;
}

} // end namespace srpc

#endif

