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

#include <list>
#include <string>
#include "workflow/WFTask.h" 
#include "rpc_module.h"

namespace srpc
{

bool RPCModule::client_task_begin(SubTask *task, const RPCModuleData& data)
{
	bool ret = this->client_begin(task, data);

	for (RPCFilter *filter : this->filters)
		ret = ret && filter->client_begin(task, data);

	return ret;
}

bool RPCModule::server_task_begin(SubTask *task, const RPCModuleData& data)
{
	bool ret = this->server_begin(task, data);

	for (RPCFilter *filter : this->filters)
		ret = ret && filter->server_begin(task, data);

	return ret;
}

bool RPCModule::client_task_end(SubTask *task, RPCModuleData& data)
{
	SubTask *filter_task;
	bool ret = this->client_end(task, data);

	for (RPCFilter *filter : this->filters)
	{
		ret = ret && filter->client_end(task, data);
		filter_task = filter->create_filter_task(data);
		series_of(task)->push_front(filter_task);
	}

	return ret;
}

bool RPCModule::server_task_end(SubTask *task, RPCModuleData& data)
{
	SubTask *filter_task;
	bool ret = this->server_end(task, data);

	for (RPCFilter *filter : this->filters)
	{
		ret = ret && filter->server_end(task, data);
		filter_task = filter->create_filter_task(data);
		series_of(task)->push_front(filter_task);
	}

	return ret;
}

SnowFlake::SnowFlake(int timestamp_bits, int group_bits, int machine_bits)
{
	this->timestamp_bits = timestamp_bits;
	this->group_bits = group_bits;
	this->machine_bits = machine_bits;
	this->sequence_bits = SRPC_TOTAL_BITS - timestamp_bits -
						  group_bits - machine_bits;

	this->last_timestamp = 1L;
	this->sequence = 0L;

	this->group_id_max = 1 << this->group_bits;
	this->machine_id_max = 1 << this->machine_bits;
	this->sequence_max = 1 << this->sequence_bits;

	this->machine_shift = this->sequence_bits;
	this->group_shift = this->machine_shift + this->machine_bits;
	this->timestamp_shift = this->group_shift + this->group_bits;
}

bool SnowFlake::get_id(long long group_id, long long machine_id,
					   unsigned long long *uid)
{
	if (group_id > this->group_id_max || machine_id > this->machine_id_max)
		return false;

	long long timestamp = GET_CURRENT_MS_STEADY();
	long long seq_id;

	if (timestamp < this->last_timestamp)
		return false;

	if (timestamp == this->last_timestamp)
	{
		if (++this->sequence > this->sequence_max)
			return false; // too many sequence_id in one millie second
	}
	else
		this->sequence = 0L;

	seq_id = this->sequence++;

	this->last_timestamp = timestamp;

	*uid = (timestamp << this->timestamp_shift) |
			(group_id << this->group_shift) |
			(machine_id << this->machine_shift) |
			seq_id;

	return true;
}

} // end namespace srpc

