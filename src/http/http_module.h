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

#ifndef __RPC_HTTP_MODULE_H__
#define __RPC_HTTP_MODULE_H__

#include "rpc_basic.h"
#include "rpc_module.h"
#include "rpc_trace_module.h"
#include "rpc_metrics_module.h"
#include "http_task.h"

namespace srpc
{

class HttpTraceModule : public TraceModule
{
public:
	bool client_begin(SubTask *task, RPCModuleData& data) const override;
	bool client_end(SubTask *task, RPCModuleData& data) const override;
	bool server_begin(SubTask *task, RPCModuleData& data) const override;
	bool server_end(SubTask *task, RPCModuleData& data) const override;
};

class HttpMetricsModule : public MetricsModule
{
public:
	bool client_begin(SubTask *task, RPCModuleData& data) const override;
	bool server_begin(SubTask *task, RPCModuleData& data) const override;
};

} // end namespace srpc

#endif

