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
static constexpr char const *SRPC_STATE				= "srpc.state";
static constexpr char const *SRPC_ERROR				= "srpc.error";
static constexpr char const *SRPC_REMOTE_IP			= "srpc.peer.ip";
static constexpr char const *SRPC_REMOTE_PORT		= "srpc.peer.port";
static constexpr char const *SRPC_SAMPLING_PRIO		= "srpc.sampling.priority";

// for srpc.component
static constexpr char const *SRPC_COMPONENT_SRPC	= "srpc.srpc";

// for ext tags
static constexpr char const *SRPC_DATA_TYPE			= "srpc.data.type";
static constexpr char const *SRPC_COMPRESS_TYPE		= "srpc.compress.type";

// Basic TraceModule for generating general span data.
// Each kind of network task can derived its own TraceModule.

template<class SERVER_TASK, class CLIENT_TASK>
class TraceModule : public RPCModule
{
public:
	bool client_begin(SubTask *task, RPCModuleData& data) override;
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_begin(SubTask *task, RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;

public:
	TraceModule() : RPCModule(RPCModuleTypeTrace)
	{
	}
};

// Fill RPC related data in trace module

template<class SERVER_TASK, class CLIENT_TASK>
class RPCTraceModule : public TraceModule<SERVER_TASK, CLIENT_TASK>
{
public:
	bool client_begin(SubTask *task, RPCModuleData& data) override;
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_begin(SubTask *task, RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;
};

////////// impl

template<class STASK, class CTASK>
bool TraceModule<STASK, CTASK>::client_begin(SubTask *task, RPCModuleData& data)
{
	if (!data.empty())
	{
		auto iter = data.find(SRPC_SPAN_ID);
		if (iter != data.end())
			data[SRPC_PARENT_SPAN_ID] = iter->second;
	} else {
		uint64_t trace_id_high = htonll(SRPCGlobal::get_instance()->get_random());
		uint64_t trace_id_low = htonll(SRPCGlobal::get_instance()->get_random());
		std::string trace_id_buf(SRPC_TRACEID_SIZE + 1, 0);

		memcpy((char *)trace_id_buf.c_str(), &trace_id_high,
				SRPC_TRACEID_SIZE / 2);
		memcpy((char *)trace_id_buf.c_str() + SRPC_TRACEID_SIZE / 2,
				&trace_id_low, SRPC_TRACEID_SIZE / 2);

		data[SRPC_TRACE_ID] = std::move(trace_id_buf);
	}

	uint64_t span_id = SRPCGlobal::get_instance()->get_random();
	std::string span_id_buf(SRPC_SPANID_SIZE + 1, 0);

	memcpy((char *)span_id_buf.c_str(), &span_id, SRPC_SPANID_SIZE);
	data[SRPC_SPAN_ID] = std::move(span_id_buf);

	data[SRPC_SPAN_KIND] = SRPC_SPAN_KIND_CLIENT;

	data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

	// clear other unnecessary module_data since the data comes from series
	data.erase(SRPC_DURATION);
	data.erase(SRPC_FINISH_TIMESTAMP);

	return true;
}

template<class STASK, class CTASK>
bool TraceModule<STASK, CTASK>::client_end(SubTask *task, RPCModuleData& data)
{
	if (data.find(SRPC_DURATION) == data.end())
	{
		unsigned long long end_time = GET_CURRENT_NS();
		data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
		data[SRPC_DURATION] = std::to_string(end_time -
							atoll(data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

template<class STASK, class CTASK>
bool TraceModule<STASK, CTASK>::server_begin(SubTask *task, RPCModuleData& data)
{
	if (data[SRPC_TRACE_ID].empty())
	{
		uint64_t trace_id_high = SRPCGlobal::get_instance()->get_random();
		uint64_t trace_id_low = SRPCGlobal::get_instance()->get_random();
		std::string trace_id_buf(SRPC_TRACEID_SIZE + 1, 0);

		memcpy((char *)trace_id_buf.c_str(), &trace_id_high,
				SRPC_TRACEID_SIZE / 2);
		memcpy((char *)trace_id_buf.c_str() + SRPC_TRACEID_SIZE / 2,
				&trace_id_low, SRPC_TRACEID_SIZE / 2);

		data[SRPC_TRACE_ID] = std::move(trace_id_buf);
	}
	else
		data[SRPC_PARENT_SPAN_ID] = data[SRPC_SPAN_ID];

	uint64_t span_id = SRPCGlobal::get_instance()->get_random();
	std::string span_id_buf(SRPC_SPANID_SIZE + 1, 0);

	memcpy((char *)span_id_buf.c_str(), &span_id, SRPC_SPANID_SIZE);
	data[SRPC_SPAN_ID] = std::move(span_id_buf);

	if (data.find(SRPC_START_TIMESTAMP) == data.end())
		data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

	data[SRPC_SPAN_KIND] = SRPC_SPAN_KIND_SERVER;

	return true;
}

template<class STASK, class CTASK>
bool TraceModule<STASK, CTASK>::server_end(SubTask *task, RPCModuleData& data)
{
	if (data.find(SRPC_DURATION) == data.end())
	{
		unsigned long long end_time = GET_CURRENT_NS();

		data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
		data[SRPC_DURATION] = std::to_string(end_time -
							atoll(data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::client_begin(SubTask *task, RPCModuleData& data)
{
	TraceModule<STASK, CTASK>::client_begin(task, data);

	auto *client_task = static_cast<CTASK *>(task);
	auto *req = client_task->get_req();

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	data[OTLP_SERVICE_NAME] = req->get_service_name();
	data[OTLP_METHOD_NAME] = req->get_method_name();
	data[SRPC_DATA_TYPE] = std::to_string(req->get_data_type());
	data[SRPC_COMPRESS_TYPE] = std::to_string(req->get_compress_type());

	return true;
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::client_end(SubTask *task, RPCModuleData& data)
{
	TraceModule<STASK, CTASK>::client_end(task, data);

	std::string ip;
	unsigned short port;
	auto *client_task = static_cast<CTASK *>(task);
	auto *resp = client_task->get_resp();

	data[SRPC_STATE] = std::to_string(resp->get_status_code());
	data[SRPC_ERROR] = std::to_string(resp->get_error());
	if (client_task->get_remote(ip, &port))
	{
		data[SRPC_REMOTE_IP] = std::move(ip);
		data[SRPC_REMOTE_PORT] = std::to_string(port);
	}

	return true;
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::server_begin(SubTask *task, RPCModuleData& data)
{
	TraceModule<STASK, CTASK>::server_begin(task, data);

	std::string ip;
	unsigned short port;
	auto *server_task = static_cast<STASK *>(task);
	auto *req = server_task->get_req();

	data[OTLP_SERVICE_NAME] = req->get_service_name();
	data[OTLP_METHOD_NAME] = req->get_method_name();
	data[SRPC_DATA_TYPE] = std::to_string(req->get_data_type());
	data[SRPC_COMPRESS_TYPE] = std::to_string(req->get_compress_type());

	data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;

	if (server_task->get_remote(ip, &port))
	{
		data[SRPC_REMOTE_IP] = std::move(ip);
		data[SRPC_REMOTE_PORT] = std::to_string(port);
	}

	return true;
}

template<class STASK, class CTASK>
bool RPCTraceModule<STASK, CTASK>::server_end(SubTask *task, RPCModuleData& data)
{
	TraceModule<STASK, CTASK>::server_end(task, data);

	auto *server_task = static_cast<STASK *>(task);
	auto *resp = server_task->get_resp();

	data[SRPC_STATE] = std::to_string(resp->get_status_code());
	data[SRPC_ERROR] = std::to_string(resp->get_error());

	return true;
}

} // end namespace srpc

#endif

