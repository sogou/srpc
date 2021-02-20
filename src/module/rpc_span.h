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

#ifndef __RPC_SPAN_H__
#define __RPC_SPAN_H__

#include <time.h>
#include <atomic>
#include <limits>
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "rpc_basic.h"
#include "rpc_module_impl.h"

namespace srpc
{

template<class RPCTYPE>
class RPCClientSpan : public RPCClientModule<RPCTYPE>
{
public:
	using TASK = RPCClientTask<typename RPCTYPE::REQ, typename RPCTYPE::RESP>;

	int begin(TASK *task, const RPCModuleData& data) override
	{
		auto *req = task->get_req();
		RPCModuleData& module_data = task->get_module_data();

		if (!data.empty())
		{
			//module_data["trace_id"] = data["trace_id"];
			auto iter = data.find("span_id");
			if (iter != data.end())
				module_data["parent_span_id"] = iter->second;
		} else {
			module_data["trace_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_trace_id());
		}

		module_data["span_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_span_id());

		module_data["service_name"] = req->get_service_name();
		module_data["method_name"] = req->get_method_name();
		module_data["data_type"] = std::to_string(req->get_data_type());
		module_data["compress_type"] = std::to_string(req->get_compress_type());
		module_data["start_time"] = std::to_string(GET_CURRENT_MS);

		return 0; // always success
	}

	//outside get data from resp->meta and call end()
	// srpc span module don`t need the resp meta module data
	int end(TASK *task, const RPCModuleData& data) override
	{
		auto *resp = task->get_resp();
		RPCModuleData& module_data = task->get_module_data();
		long long end_time = GET_CURRENT_MS;

		module_data["end_time"] = std::to_string(end_time);
		module_data["cost"] = std::to_string(end_time -
											 atoll(module_data["start_time"].c_str()));
		module_data["state"] = std::to_string(resp->get_status_code());
		module_data["error"] = std::to_string(resp->get_error());
		module_data["remote_ip"] = task->get_remote_ip();

		// whether create a module task is depend on you and your derived class
		SubTask *module_task = this->create_module_task(module_data);
		series_of(task)->push_front(module_task);

		return 0;
	}
};

template<class RPCTYPE>
class RPCServerSpan : public RPCServerModule<RPCTYPE>
{
public:
	using TASK = RPCServerTask<typename RPCTYPE::REQ, typename RPCTYPE::RESP>;

	// outside will fill data from meta and call begin() and put data onto series
	int begin(TASK *task, const RPCModuleData& data) override
	{
		auto *req = task->get_req();
		RPCModuleData& module_data = task->get_module_data();

		module_data["service_name"] = req->get_service_name();
		module_data["method_name"] = req->get_method_name();
		module_data["data_type"] = std::to_string(req->get_data_type());
		module_data["compress_type"] = std::to_string(req->get_compress_type());

		if (module_data["trace_id"].empty())
			module_data["trace_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_trace_id());
		module_data["span_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_span_id());
		module_data["start_time"] = std::to_string(GET_CURRENT_MS);

//		RPCSeriesWork *series = dynamic_cast<RPCSeriesWork *>(series_of(task));
//		if (series)
//			series->set_data(task_data);

		return 0;
	}

	int end(TASK *task, const RPCModuleData *data) override
	{
		auto *resp = task->get_resp();
		RPCModuleData& module_data = task->get_module_data();
		long long end_time = GET_CURRENT_MS;

		module_data["end_time"] = std::to_string(end_time);
		module_data["cost"] = std::to_string(end_time -
											 atoll(module_data["start_time"].c_str()));
		module_data["state"] = std::to_string(resp->get_status_code());
		module_data["error"] = std::to_string(resp->get_error());
		module_data["remote_ip"] = task->get_remote_ip();

		// whether create a module task is depend on you and your derived class
		SubTask *module_task = this->create_module_task(module_data);
		series_of(task)->push_front(module_task);

//		RPCSeriesWork *series = dynamic_cast<RPCSeriesWork *>(series_of(this));
//		if (series)
//			series->clear_span();

		return 0;
	}
};

} // end namespace srpc

#endif
