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

#include "rpc_metrics_module.h"

namespace srpc
{

bool MetricsModule::client_begin(SubTask *task, RPCModuleData& data)
{
	data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());
	// clear other unnecessary module_data since the data comes from series
	data.erase(SRPC_DURATION);
	data.erase(SRPC_FINISH_TIMESTAMP);

	return true;
}

bool MetricsModule::client_end(SubTask *task, RPCModuleData& data)
{
	if (data.find(SRPC_START_TIMESTAMP) != data.end() &&
		data.find(SRPC_DURATION) == data.end())
	{
		unsigned long long end_time = GET_CURRENT_NS();
		data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
		data[SRPC_DURATION] = std::to_string(end_time -
							atoll(data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

bool MetricsModule::server_begin(SubTask *task, RPCModuleData& data)
{
	if (data.find(SRPC_START_TIMESTAMP) == data.end())
		data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());

	return true;
}

bool MetricsModule::server_end(SubTask *task, RPCModuleData& data)
{
	if (data.find(SRPC_START_TIMESTAMP) != data.end() &&
		data.find(SRPC_DURATION) == data.end())
	{
		unsigned long long end_time = GET_CURRENT_NS();

		data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
		data[SRPC_DURATION] = std::to_string(end_time -
							atoll(data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

} // end namespace srpc

