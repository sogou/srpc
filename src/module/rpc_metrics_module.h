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

#ifndef __RPC_METRICS_MODULE_H__
#define __RPC_METRICS_MODULE_H__

#include "rpc_basic.h"
#include "rpc_context.h"
#include "rpc_module.h"

namespace srpc
{

// Basic MetricsModlue for generating general metrics data.
// Each kind of network task can derived its own MetricsModlue.

template<class SERVER_TASK, class CLIENT_TASK>
class MetricsModule : public RPCModule
{
public:
	bool client_begin(SubTask *task, const RPCModuleData& data) override;
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_begin(SubTask *task, const RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;

public:
	MetricsModule() : RPCModule(RPCModuleTypeMetrics) { }
};

// Fill RPC related data in metrics module

template<class SERVER_TASK, class CLIENT_TASK>
class RPCMetricsModule : public MetricsModule<SERVER_TASK, CLIENT_TASK>
{
public:
	bool client_begin(SubTask *task, const RPCModuleData& data) override;
	bool server_begin(SubTask *task, const RPCModuleData& data) override;
};

////////// impl

template<class STASK, class CTASK>
bool MetricsModule<STASK, CTASK>::client_begin(SubTask *task,
											   const RPCModuleData& data)
{
	auto *client_task = static_cast<CTASK *>(task);
	RPCModuleData& module_data = *(client_task->mutable_module_data());

	if (module_data.find(SRPC_START_TIMESTAMP) == module_data.end())
		module_data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

	return true;
}

template<class STASK, class CTASK>
bool MetricsModule<STASK, CTASK>::client_end(SubTask *task, RPCModuleData& data)
{
	auto *client_task = static_cast<CTASK *>(task);
	RPCModuleData& module_data = *(client_task->mutable_module_data());

	if (module_data.find(SRPC_DURATION) == module_data.end())
	{
		module_data[SRPC_DURATION] = std::to_string(GET_CURRENT_NS() -
					atoll(module_data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

template<class STASK, class CTASK>
bool MetricsModule<STASK, CTASK>::server_begin(SubTask *task,
											   const RPCModuleData& data)
{
	auto *server_task = static_cast<STASK *>(task);
	RPCModuleData& module_data = *(server_task->mutable_module_data());

	if (module_data.find(SRPC_START_TIMESTAMP) == module_data.end())
		module_data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

	return true;
}

template<class STASK, class CTASK>
bool MetricsModule<STASK, CTASK>::server_end(SubTask *task, RPCModuleData& data)
{
	auto *server_task = static_cast<STASK *>(task);
	RPCModuleData& module_data = *(server_task->mutable_module_data());

	if (module_data.find(SRPC_DURATION) == module_data.end())
	{
		module_data[SRPC_DURATION] = std::to_string(GET_CURRENT_NS() -
					atoll(module_data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

template<class STASK, class CTASK>
bool RPCMetricsModule<STASK, CTASK>::client_begin(SubTask *task,
												  const RPCModuleData& data)
{
	MetricsModule<STASK, CTASK>::client_begin(task, data);

	auto *client_task = static_cast<CTASK *>(task);
	auto *req = client_task->get_req();
	RPCModuleData& module_data = *(client_task->mutable_module_data());

	module_data[OTLP_SERVICE_NAME] = req->get_service_name();
	module_data[OTLP_METHOD_NAME] = req->get_method_name();

	return true;
}

template<class STASK, class CTASK>
bool RPCMetricsModule<STASK, CTASK>::server_begin(SubTask *task,
												  const RPCModuleData& data)
{
	MetricsModule<STASK, CTASK>::server_begin(task, data);

	auto *server_task = static_cast<STASK *>(task);
	auto *req = server_task->get_req();
	RPCModuleData& module_data = *(server_task->mutable_module_data());

	module_data[OTLP_SERVICE_NAME] = req->get_service_name();
	module_data[OTLP_METHOD_NAME] = req->get_method_name();

	return true;
}

} // end namespace srpc

#endif

