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
#include "workflow/WFTask.h"
#include "rpc_basic.h"
#include "rpc_module.h"
#include "rpc_context.h"

namespace srpc
{

const char *const SRPC_MODULE_SPAN_ID			= "srpc_module_span_id";
const char *const SRPC_MODULE_TRACE_ID			= "srpc_module_trace_id";
const char *const SRPC_MODULE_PARENT_SPAN_ID	= "srpc_module_parent_span_id";
const char *const SRPC_MODULE_SERVICE_NAME		= "srpc_module_service_name";
const char *const SRPC_MODULE_METHOD_NAME		= "srpc_module_method_name";
const char *const SRPC_MODULE_DATA_TYPE			= "srpc_module_data_type";
const char *const SRPC_MODULE_COMPRESS_TYPE		= "srpc_module_compress_type";
const char *const SRPC_MODULE_START_TIME		= "srpc_module_start_time";
const char *const SRPC_MODULE_END_TIME			= "srpc_module_end_time";
const char *const SRPC_MODULE_COST				= "srpc_module_cost";
const char *const SRPC_MODULE_STATE				= "srpc_module_state";
const char *const SRPC_MODULE_ERROR				= "srpc_module_error";
const char *const SRPC_MODULE_REMOTE_IP			= "srpc_module_remote_ip";

template<class RPCTYPE>
class RPCSpanModule : public RPCModule<typename RPCTYPE::REQ,
									   typename RPCTYPE::RESP>
{
public:
	using CLIENT_TASK = RPCClientTask<typename RPCTYPE::REQ,
									  typename RPCTYPE::RESP>;
	using SERVER_TASK = RPCServerTask<typename RPCTYPE::REQ,
									  typename RPCTYPE::RESP>;

	int client_begin(SubTask *task, const RPCModuleData& data) override
	{
		auto *client_task = static_cast<CLIENT_TASK *>(task);
		auto *req = client_task->get_req();
		RPCModuleData& module_data = *(client_task->mutable_module_data());

		if (!data.empty())
		{
			//module_data["trace_id"] = data["trace_id"];
			auto iter = data.find(SRPC_MODULE_SPAN_ID);
			if (iter != data.end())
				module_data[SRPC_MODULE_PARENT_SPAN_ID] = iter->second;
		} else {
			module_data[SRPC_MODULE_TRACE_ID] = std::to_string(
								SRPCGlobal::get_instance()->get_trace_id());
		}

		module_data[SRPC_MODULE_SPAN_ID] = std::to_string(
								SRPCGlobal::get_instance()->get_span_id());

		module_data[SRPC_MODULE_SERVICE_NAME] = req->get_service_name();
		module_data[SRPC_MODULE_METHOD_NAME] = req->get_method_name();
		module_data[SRPC_MODULE_DATA_TYPE] = std::to_string(req->get_data_type());
		module_data[SRPC_MODULE_COMPRESS_TYPE] =
										std::to_string(req->get_compress_type());
		module_data[SRPC_MODULE_START_TIME] = std::to_string(GET_CURRENT_MS);

		return 0; // always success
	}

	int client_end(SubTask *task, const RPCModuleData& data) override
	{
		auto *client_task = static_cast<CLIENT_TASK *>(task);
		auto *resp = client_task->get_resp();
		RPCModuleData& module_data = *(client_task->mutable_module_data());
		long long end_time = GET_CURRENT_MS;

		module_data[SRPC_MODULE_END_TIME] = std::to_string(end_time);
		module_data[SRPC_MODULE_COST] = std::to_string(end_time -
							atoll(module_data[SRPC_MODULE_START_TIME].c_str()));
		module_data[SRPC_MODULE_STATE] = std::to_string(resp->get_status_code());
		module_data[SRPC_MODULE_ERROR] = std::to_string(resp->get_error());
		module_data[SRPC_MODULE_REMOTE_IP] = client_task->get_remote_ip();

		SubTask *module_task = this->create_module_task(module_data);
		series_of(task)->push_front(module_task);

		return 0;
	}

	int server_begin(SubTask *task, const RPCModuleData& data) override
	{
		auto *server_task = static_cast<SERVER_TASK *>(task);
		auto *req = server_task->get_req();
		RPCModuleData& module_data = *(server_task->mutable_module_data());

		module_data[SRPC_MODULE_SERVICE_NAME] = req->get_service_name();
		module_data[SRPC_MODULE_METHOD_NAME] = req->get_method_name();
		module_data[SRPC_MODULE_DATA_TYPE] = std::to_string(req->get_data_type());
		module_data[SRPC_MODULE_COMPRESS_TYPE] =
										std::to_string(req->get_compress_type());

		if (module_data[SRPC_MODULE_TRACE_ID].empty())
			module_data[SRPC_MODULE_TRACE_ID] = std::to_string(
								SRPCGlobal::get_instance()->get_trace_id());
		module_data[SRPC_MODULE_SPAN_ID] = std::to_string(
								SRPCGlobal::get_instance()->get_span_id());
		module_data[SRPC_MODULE_START_TIME] = std::to_string(GET_CURRENT_MS);

		return 0;
	}

	int server_end(SubTask *task, const RPCModuleData& data) override
	{
		auto *server_task = static_cast<SERVER_TASK *>(task);
		auto *resp = server_task->get_resp();
		RPCModuleData& module_data = *(server_task->mutable_module_data());
		long long end_time = GET_CURRENT_MS;

		module_data[SRPC_MODULE_END_TIME] = std::to_string(end_time);
		module_data[SRPC_MODULE_COST] = std::to_string(end_time -
							atoll(module_data[SRPC_MODULE_START_TIME].c_str()));
		module_data[SRPC_MODULE_STATE] = std::to_string(resp->get_status_code());
		module_data[SRPC_MODULE_ERROR] = std::to_string(resp->get_error());
		module_data[SRPC_MODULE_REMOTE_IP] = server_task->get_remote_ip();

		SubTask *module_task = this->create_module_task(module_data);
		series_of(task)->push_front(module_task);

		return 0;
	}
};

} // end namespace srpc

#endif
