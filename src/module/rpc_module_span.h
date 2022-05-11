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

#ifndef __RPC_MODULE_SPAN_H__
#define __RPC_MODULE_SPAN_H__

#include <time.h>
#include <atomic>
#include <limits>
#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <arpa/inet.h>
#include "workflow/WFTask.h"
#include "rpc_basic.h"
#include "rpc_module.h"
#include "rpc_context.h"

namespace srpc
{

// follows the basic conventions of RPC part in opentracing
static constexpr char const *SRPC_COMPONENT			= "srpc.component";
static constexpr char const *SRPC_COMPONENT_SRPC	= "srpc.srpc";
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

// SRPC ext tags
static constexpr char const *SRPC_DATA_TYPE			= "srpc.data.type";
static constexpr char const *SRPC_COMPRESS_TYPE		= "srpc.compress.type";

template<class RPCTYPE>
class RPCSpanModule : public RPCModule
{
public:
	using CLIENT_TASK = RPCClientTask<typename RPCTYPE::REQ,
									  typename RPCTYPE::RESP>;
	using SERVER_TASK = RPCServerTask<typename RPCTYPE::REQ,
									  typename RPCTYPE::RESP>;

	bool client_begin(SubTask *task, const RPCModuleData& data) override;
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_begin(SubTask *task, const RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;

public:
	RPCSpanModule() : RPCModule(RPCModuleTypeSpan)
	{
	}
};

////////// impl

template<class RPCTYPE>
bool RPCSpanModule<RPCTYPE>::client_begin(SubTask *task,
										  const RPCModuleData& data)
{
	auto *client_task = static_cast<CLIENT_TASK *>(task);
	auto *req = client_task->get_req();
	RPCModuleData& module_data = *(client_task->mutable_module_data());

	if (!data.empty())
	{
		auto iter = data.find(SRPC_SPAN_ID);
		if (iter != data.end())
			module_data[SRPC_PARENT_SPAN_ID] = iter->second;
	} else {
		uint64_t trace_id_high = htonll(SRPCGlobal::get_instance()->get_random());
		uint64_t trace_id_low = htonll(SRPCGlobal::get_instance()->get_random());
		std::string trace_id_buf(SRPC_TRACEID_SIZE + 1, 0);

		memcpy((char *)trace_id_buf.c_str(), &trace_id_high,
				SRPC_TRACEID_SIZE / 2);
		memcpy((char *)trace_id_buf.c_str() + SRPC_TRACEID_SIZE / 2,
				&trace_id_low, SRPC_TRACEID_SIZE / 2);

		module_data[SRPC_TRACE_ID] = std::move(trace_id_buf);
	}

	uint64_t span_id = SRPCGlobal::get_instance()->get_random();
	std::string span_id_buf(SRPC_SPANID_SIZE + 1, 0);

	memcpy((char *)span_id_buf.c_str(), &span_id, SRPC_SPANID_SIZE);
	module_data[SRPC_SPAN_ID] = std::move(span_id_buf);

	module_data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	module_data[SRPC_SPAN_KIND] = SRPC_SPAN_KIND_CLIENT;

	module_data[SRPC_SERVICE_NAME] = req->get_service_name();
	module_data[SRPC_METHOD_NAME] = req->get_method_name();
	module_data[SRPC_DATA_TYPE] = std::to_string(req->get_data_type());
	module_data[SRPC_COMPRESS_TYPE] =
							std::to_string(req->get_compress_type());
	if (module_data.find(SRPC_START_TIMESTAMP) == module_data.end())
		module_data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());
	return true; // always success
}

template<class RPCTYPE>
bool RPCSpanModule<RPCTYPE>::client_end(SubTask *task,
										RPCModuleData& data)
{
	std::string ip;
	unsigned short port;
	auto *client_task = static_cast<CLIENT_TASK *>(task);
	auto *resp = client_task->get_resp();
	RPCModuleData& module_data = *(client_task->mutable_module_data());
	unsigned long long end_time = GET_CURRENT_NS();

	for (auto kv : module_data)
		data[kv.first] = module_data[kv.first];

	data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
	data[SRPC_DURATION] = std::to_string(end_time -
						atoll(data[SRPC_START_TIMESTAMP].data()));
	data[SRPC_STATE] = std::to_string(resp->get_status_code());
	data[SRPC_ERROR] = std::to_string(resp->get_error());
	if (client_task->get_remote(ip, &port))
	{
		data[SRPC_REMOTE_IP] = std::move(ip);
		data[SRPC_REMOTE_PORT] = std::to_string(port);
	}

	return true;
}

template<class RPCTYPE>
bool RPCSpanModule<RPCTYPE>::server_begin(SubTask *task,
										  const RPCModuleData& data)
{
	std::string ip;
	unsigned short port;
	auto *server_task = static_cast<SERVER_TASK *>(task);
	auto *req = server_task->get_req();
	RPCModuleData& module_data = *(server_task->mutable_module_data());

	module_data[SRPC_SERVICE_NAME] = req->get_service_name();
	module_data[SRPC_METHOD_NAME] = req->get_method_name();
	module_data[SRPC_DATA_TYPE] = std::to_string(req->get_data_type());
	module_data[SRPC_COMPRESS_TYPE] =
							std::to_string(req->get_compress_type());

	if (module_data[SRPC_TRACE_ID].empty())
	{
		uint64_t trace_id_high = SRPCGlobal::get_instance()->get_random();
		uint64_t trace_id_low = SRPCGlobal::get_instance()->get_random();
		std::string trace_id_buf(SRPC_TRACEID_SIZE + 1, 0);

		memcpy((char *)trace_id_buf.c_str(), &trace_id_high,
				SRPC_TRACEID_SIZE / 2);
		memcpy((char *)trace_id_buf.c_str() + SRPC_TRACEID_SIZE / 2,
				&trace_id_low, SRPC_TRACEID_SIZE / 2);

		module_data[SRPC_TRACE_ID] = std::move(trace_id_buf);
	}
	else
		module_data[SRPC_PARENT_SPAN_ID] = module_data[SRPC_SPAN_ID];

	uint64_t span_id = SRPCGlobal::get_instance()->get_random();
	std::string span_id_buf(SRPC_SPANID_SIZE + 1, 0);

	memcpy((char *)span_id_buf.c_str(), &span_id, SRPC_SPANID_SIZE);
	module_data[SRPC_SPAN_ID] = std::move(span_id_buf);

	if (module_data.find(SRPC_START_TIMESTAMP) == module_data.end())
		module_data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

	module_data[SRPC_COMPONENT] = SRPC_COMPONENT_SRPC;
	module_data[SRPC_SPAN_KIND] = SRPC_SPAN_KIND_SERVER;

	if (server_task->get_remote(ip, &port))
	{
		module_data[SRPC_REMOTE_IP] = std::move(ip);
		module_data[SRPC_REMOTE_PORT] = std::to_string(port);
	}

	return true;
}

template<class RPCTYPE>
bool RPCSpanModule<RPCTYPE>::server_end(SubTask *task,
										RPCModuleData& data)
{
	auto *server_task = static_cast<SERVER_TASK *>(task);
	auto *resp = server_task->get_resp();
	RPCModuleData& module_data = *(server_task->mutable_module_data());
	unsigned long long end_time = GET_CURRENT_NS();

	module_data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
	module_data[SRPC_DURATION] = std::to_string(end_time -
						atoll(module_data[SRPC_START_TIMESTAMP].data()));
	module_data[SRPC_STATE] = std::to_string(resp->get_status_code());
	module_data[SRPC_ERROR] = std::to_string(resp->get_error());

	return true;
}

} // end namespace srpc

#endif

