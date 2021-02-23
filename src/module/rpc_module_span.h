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
#include "rpc_span_policies.h"

namespace srpc
{
template<class RPCTYPE>
class RPCClientSpanDefault : public RPCSpanDefault, public RPCClientSpan<RPCTYPE>
{
public:
	SubTask* create_module_task(const RPCModuleData& data) override
	{
		return this->create_span_task(const_cast<RPCModuleData&>(data));
	}
};

template<class RPCTYPE>
class RPCServerSpanDefault : public RPCSpanDefault, public RPCServerSpan<RPCTYPE>
{
public:
	SubTask* create_module_task(const RPCModuleData& data) override
	{
		return this->create_span_task(const_cast<RPCModuleData&>(data));
	}
};

template<class RPCTYPE>
class RPCClientSpanRedis : public RPCSpanRedis, public RPCClientSpan<RPCTYPE>
{
public:
	SubTask* create_module_task(const RPCModuleData& data) override
	{
		return this->create_span_task(const_cast<RPCModuleData&>(data));
	}
};

template<class RPCTYPE>
class RPCServerSpanRedis : public RPCSpanRedis, public RPCServerSpan<RPCTYPE>
{
public:
	SubTask* create_module_task(const RPCModuleData& data) override
	{
		return this->create_span_task(const_cast<RPCModuleData&>(data));
	}
};

} // end namespace srpc

#endif
