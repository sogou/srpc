/*
  Copyright (c) 2021 Sogou, Inc.

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

#ifndef __RPC_FILTER_H__
#define __RPC_FILTER_H__

#include <map>
#include <vector>
#include <string>
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"

namespace srpc
{
static constexpr unsigned int	OTLP_HTTP_REDIRECT_MAX		= 0;
static constexpr unsigned int	OTLP_HTTP_RETRY_MAX			= 1;
static constexpr const char	   *OTLP_SERVICE_NAME			= "service.name";
static constexpr const char	   *OTLP_METHOD_NAME			= "operation.name";
static constexpr size_t			RPC_REPORT_THREHOLD_DEFAULT	= 100;
static constexpr size_t			RPC_REPORT_INTERVAL_DEFAULT	= 1000; /* msec */

using RPCModuleData = std::map<std::string, std::string>;

static RPCModuleData global_empty_map;

class RPCFilter
{
public:
	SubTask *create_filter_task(const RPCModuleData& data)
	{
		if (this->filter(const_cast<RPCModuleData&>(data)))
			return this->create(const_cast<RPCModuleData&>(data));

		return WFTaskFactory::create_empty_task();
	}

private:
	virtual SubTask *create(RPCModuleData& data) = 0;
	virtual bool filter(RPCModuleData& data) = 0;

public:
	RPCFilter(enum RPCModuleType module_type)
	{
		this->module_type = module_type;
	}

	virtual ~RPCFilter() { }

	enum RPCModuleType get_module_type() const { return this->module_type; }

	virtual bool client_begin(SubTask *task, RPCModuleData& data)
	{
		return true;
	}
	virtual bool server_begin(SubTask *task, RPCModuleData& data)
	{
		return true;
	}
	virtual bool client_end(SubTask *task, RPCModuleData& data)
	{
		return true;
	}
	virtual bool server_end(SubTask *task, RPCModuleData& data)
	{
		return true;
	}

private:
	enum RPCModuleType module_type;
};

} // end namespace srpc

#endif

