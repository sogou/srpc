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
		return true;
	}
	bool client_end(SubTask *task, RPCModuleData& data) override
	{
		return true;
	}
	bool server_begin(SubTask *task, const RPCModuleData& data) override
	{
		return true;
	}
	bool server_end(SubTask *task, RPCModuleData& data) override
	{
		return true;
	}

public:
	RPCMetricsModule() : RPCModule(RPCModuleTypeMetrics) { }
};

} // end namespace srpc

#endif

