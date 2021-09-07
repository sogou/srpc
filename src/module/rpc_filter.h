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

using RPCModuleData = std::map<std::string, std::string>;
using RPCLogVector = std::vector<std::pair<std::string, std::string>>;

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
	RPCFilter(int module_type)
	{
		this->module_type = module_type;
	}

	int get_module_type() const { return this->module_type; }

	virtual bool client_begin(SubTask *task, const RPCModuleData& data)
	{
		return true;
	}
	virtual bool server_begin(SubTask *task, const RPCModuleData& data)
	{
		return true;
	}
	virtual bool client_end(SubTask *task, const RPCModuleData& data)
	{
		return true;
	}
	virtual bool server_end(SubTask *task, const RPCModuleData& data)
	{
		return true;
	}

private:
	int module_type;
};

} // end namespace srpc

#endif

