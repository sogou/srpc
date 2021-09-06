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

#ifndef __RPC_MODULE_H__
#define __RPC_MODULE_H__

#include <mutex>
#include <string>
#include <unordered_map>
#include "workflow/WFTask.h" 
#include "workflow/WFTaskFactory.h"
#include "rpc_filter.h"

namespace srpc
{

const char *const SRPC_SPAN_LOG			= "log";
const char *const SRPC_SPAN_EVENT		= "event";
const char *const SRPC_SPAN_MESSAGE		= "message";

class RPCModule
{
protected:
	virtual bool client_begin(SubTask *task, const RPCModuleData& data) = 0;
	virtual bool server_begin(SubTask *task, const RPCModuleData& data) = 0;
	virtual bool client_end(SubTask *task, const RPCModuleData& data) = 0;
	virtual bool server_end(SubTask *task, const RPCModuleData& data) = 0;

public:
	bool client_task_begin(SubTask *task, const RPCModuleData& data)
	{
		bool ret = this->client_begin(task, data);

		for (const auto& filter_pair : this->filters)
			ret &= filter_pair.second->client_begin(task, data);

		return ret;
	}

	bool server_task_begin(SubTask *task, const RPCModuleData& data)
	{
		bool ret = this->server_begin(task, data);

		for (const auto& filter_pair : this->filters)
			ret &= filter_pair.second->server_begin(task, data);

		return ret;
	}

	bool client_task_end(SubTask *task, const RPCModuleData& data)
	{
		SubTask *filter_task;
		bool ret = this->client_end(task, data);

		for (const auto& filter_pair : this->filters)
		{
			ret &= filter_pair.second->client_end(task, data);
			filter_task = filter_pair.second->create_filter_task(data);
			series_of(task)->push_front(filter_task);
		}

		return ret;
	}

	bool server_task_end(SubTask *task, const RPCModuleData& data)
	{
		SubTask *filter_task;
		bool ret = this->server_end(task, data);

		for (const auto& filter_pair : this->filters)
		{
			ret &= filter_pair.second->server_end(task, data);
			filter_task = filter_pair.second->create_filter_task(data);
			series_of(task)->push_front(filter_task);
		}

		return ret;
	}

	bool add_filter(RPCFilter *filter)
	{
		this->mutex.lock();
		auto iter = this->filters.find(filter->get_name());

		if (iter == this->filters.end())
			this->filters.insert(std::make_pair(filter->get_name(), filter));

		this->mutex.unlock();
		return iter == this->filters.end();
	}
/*
	bool remove_filter(const std::string& name)
	{
		this->mutex.lock();
		auto iter = this->filters.find(name);

		if (iter != this->filters.end())
			this->filters.erase(iter);

		this->mutex.unlock();
		return iter != this->filters.end();
	}
*/
	size_t get_filters_size() const { return this->filters.size(); }
	RPCModuleType get_module_type() const { return this->module_type; }
	RPCModule(RPCModuleType module_type) { this->module_type = module_type; }
	virtual ~RPCModule() {}

private:
	RPCModuleType module_type;
	std::mutex mutex;
	std::unordered_map<std::string, RPCFilter *> filters;
};

class SnowFlake
{
public:
	SnowFlake() :
		SnowFlake(TIMESTAMP_BITS, GROUP_BITS, MACHINE_BITS)
	{
	}

	SnowFlake(int timestamp_bits,
			  int group_bits,
			  int machine_bits)
	{
		this->timestamp_bits = timestamp_bits;
		this->group_bits = group_bits;
		this->machine_bits = machine_bits;
		this->sequence_bits = TOTAL_BITS - timestamp_bits - group_bits - machine_bits;

		this->last_timestamp = 1L;
		this->sequence = 0L;

		this->group_id_max = 1 << this->group_bits;
		this->machine_id_max = 1 << this->machine_bits;
		this->sequence_max = 1 << this->sequence_bits;

		this->machine_shift = this->sequence_bits;
		this->group_shift = this->machine_shift + this->machine_bits;
		this->timestamp_shift = this->group_shift + this->group_bits;
	}

	bool get_id(long long group_id, long long machine_id, long long *uid)
	{
		if (group_id > this->group_id_max || machine_id > this->machine_id_max)
			return false;

		long long timestamp = GET_CURRENT_MS_STEADY;
		long long seq_id;

		if (timestamp < this->last_timestamp)
			return false;

		if (timestamp == this->last_timestamp)
		{
			if (++this->sequence > this->sequence_max)
				return false; // too many sequence_id in one millie second
		} else {
			this->sequence = 0L;
		}
		seq_id = this->sequence++;

		this->last_timestamp = timestamp;

		*uid = (timestamp << this->timestamp_shift) |
				(group_id << this->group_shift) |
				(machine_id << this->machine_shift) |
				seq_id;

		return true;
	}

private:
	std::atomic<long long> last_timestamp; // -1L;
	std::atomic<long long> sequence; // 0L;

	int timestamp_bits;
	int group_bits;
	int machine_bits;
	int sequence_bits;

	long long group_id_max;
	long long machine_id_max;
	long long sequence_max;

	long long timestamp_shift;
	long long group_shift;
	long long machine_shift;

	// u_id = [timestamp][group][machine][sequence]
	static constexpr int TIMESTAMP_BITS = 37;
	static constexpr int GROUP_BITS = 4;
	static constexpr int MACHINE_BITS = 10;
//	static constexpr int SEQUENCE_BITS = 12;
	static constexpr int TOTAL_BITS = 63;
};

} // end namespace srpc

#endif

