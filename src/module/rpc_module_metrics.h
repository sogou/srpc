/*
  Copyright (c) 2022 Sogou, Inc.

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

#ifndef __RPC_MODULE_METRICS_H__
#define __RPC_MODULE_METRICS_H__

#include "rpc_basic.h"
#include "rpc_module.h"
#include "rpc_context.h"

namespace srpc
{

template<class RPCTYPE>
class RPCMetricsModule : public RPCModule
{
public:
	using CLIENT_TASK = RPCClientTask<typename RPCTYPE::REQ,
									  typename RPCTYPE::RESP>;
	using SERVER_TASK = RPCServerTask<typename RPCTYPE::REQ,
									  typename RPCTYPE::RESP>;

	bool client_begin(SubTask *task, const RPCModuleData& data) override
	{
		auto *client_task = static_cast<CLIENT_TASK *>(task);
		auto *req = client_task->get_req();
		RPCModuleData& module_data = *(client_task->mutable_module_data());	

		module_data[SRPC_SERVICE_NAME] = req->get_service_name();
		module_data[SRPC_METHOD_NAME] = req->get_method_name();
		if (module_data.find(SRPC_START_TIMESTAMP) == module_data.end())
			module_data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

		return true;
	}

	bool client_end(SubTask *task, RPCModuleData& data) override
	{
		auto *client_task = static_cast<CLIENT_TASK *>(task);
		RPCModuleData& module_data = *(client_task->mutable_module_data());

		if (module_data.find(SRPC_DURATION) == module_data.end())
		{
			module_data[SRPC_DURATION] = std::to_string(GET_CURRENT_NS() -
						atoll(module_data[SRPC_START_TIMESTAMP].data()));
		}

		return true;
	}

	bool server_begin(SubTask *task, const RPCModuleData& data) override
	{
		auto *server_task = static_cast<SERVER_TASK *>(task);
		auto *req = server_task->get_req();
		RPCModuleData& module_data = *(server_task->mutable_module_data());

		module_data[SRPC_SERVICE_NAME] = req->get_service_name();
		module_data[SRPC_METHOD_NAME] = req->get_method_name();
		if (module_data.find(SRPC_START_TIMESTAMP) == module_data.end())
			module_data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

		return true;
	}

	bool server_end(SubTask *task, RPCModuleData& data) override
	{
		auto *server_task = static_cast<SERVER_TASK *>(task);
		RPCModuleData& module_data = *(server_task->mutable_module_data());

		if (module_data.find(SRPC_DURATION) == module_data.end())
		{
			module_data[SRPC_DURATION] = std::to_string(GET_CURRENT_NS() -
						atoll(module_data[SRPC_START_TIMESTAMP].data()));
		}

		return true;
	}

public:
	RPCMetricsModule() : RPCModule(RPCModuleTypeMetrics) { }
};

} // end namespace srpc

#endif

