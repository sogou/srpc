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
// for transfer between client-server
static constexpr const char	   *OTLP_TRACE_PARENT			= "traceparent";
static constexpr const char	   *OTLP_TRACE_STATE			= "tracestate";
static constexpr const char	   *OTLP_AFFINITY_ATTRIBUTE		= "affinity_attribute";
static constexpr size_t			OTLP_TRACE_VERSION_SIZE		= 2;
static constexpr char const    *OTLP_TRACE_VERSION			= "trace_version";
static constexpr char const    *OTLP_TRACE_FLAG				= "trace_flag";

// for report
static constexpr unsigned int	OTLP_HTTP_REDIRECT_MAX		= 0;
static constexpr unsigned int	OTLP_HTTP_RETRY_MAX			= 1;
static constexpr const char	   *OTLP_SERVICE_NAME			= "rpc.service";
static constexpr const char	   *OTLP_METHOD_NAME			= "rpc.method";
static constexpr const char	   *SRPC_HTTP_METHOD			= "http.method";
static constexpr const char	   *SRPC_HTTP_STATUS_CODE		= "http.status_code";

static constexpr size_t			RPC_REPORT_THREHOLD_DEFAULT	= 100;
static constexpr size_t			RPC_REPORT_INTERVAL_DEFAULT	= 1000; /* msec */
static constexpr const char	   *SRPC_MODULE_DATA			= "srpc_module_data";

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

	RPCFilter(const std::string name, enum RPCModuleType module_type)
	{
		size_t pos = name.find("::");
		if (pos != std::string::npos)
			this->filter_name = name.substr(0, pos) + "::";
		else
			this->filter_name = name + "::";
		this->module_type = module_type;
	}

	virtual ~RPCFilter() { }

	enum RPCModuleType get_module_type() const { return this->module_type; }
	const std::string& get_name() const { return this->filter_name; }

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

	static const std::string raw_var_name(const std::string& name)
	{
		size_t pos = name.find("::");
		if (pos != std::string::npos)
			return name.substr(pos + 2);
		else
			return name;
	}

private:
	enum RPCModuleType module_type;
	std::string filter_name;
};

} // end namespace srpc

#endif

