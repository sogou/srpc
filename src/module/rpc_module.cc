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
#include <workflow/HttpUtil.h>
#include "rpc_basic.h"
#include "rpc_module.h"
#include "rpc_trace_module.h"

namespace srpc
{

bool RPCModule::client_task_begin(SubTask *task, RPCModuleData& data)
{
	bool ret = this->client_begin(task, data);

	for (RPCFilter *filter : this->filters)
		ret = ret && filter->client_begin(task, data);

	return ret;
}

bool RPCModule::server_task_begin(SubTask *task, RPCModuleData& data)
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
		if (filter->client_end(task, data))
		{
			filter_task = filter->create_filter_task(data);
			series_of(task)->push_front(filter_task);
		}
		else
			ret = false;
	}

	return ret;
}

bool RPCModule::server_task_end(SubTask *task, RPCModuleData& data)
{
	SubTask *filter_task;
	bool ret = this->server_end(task, data);

	for (RPCFilter *filter : this->filters)
	{
		if (filter->server_end(task, data))
		{
			filter_task = filter->create_filter_task(data);
			series_of(task)->push_front(filter_task);
		}
		else
			ret = false;
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

bool http_set_header_module_data(const RPCModuleData& data,
								 protocol::HttpMessage *msg)
{
	auto it = data.begin();
	int flag = 0;

	while (it != data.end() && flag != 3)
	{
		if (it->first == SRPC_TRACE_ID)
		{
			char trace_id_buf[SRPC_TRACEID_SIZE * 2 + 1];
			TRACE_ID_BIN_TO_HEX((uint64_t *)it->second.data(), trace_id_buf);
			msg->set_header_pair("Trace-Id", trace_id_buf);
			flag |= 1;
		}
		else if (it->first == SRPC_SPAN_ID)
		{
			char span_id_buf[SRPC_SPANID_SIZE * 2 + 1];
			SPAN_ID_BIN_TO_HEX((uint64_t *)it->second.data(), span_id_buf);
			msg->set_header_pair("Span-Id", span_id_buf);
			flag |= (1 << 1);
		}
		++it;
	}

	return true;
}

bool http_get_header_module_data(const protocol::HttpMessage *msg,
								 RPCModuleData& data)
{
	protocol::HttpHeaderCursor cursor(msg);
	std::string name;
	std::string value;
	int flag = 0;

	while (cursor.next(name, value) && flag != 3)
	{
		if (strcasecmp(name.c_str(), "Trace-Id") == 0 &&
			value.size() == SRPC_TRACEID_SIZE * 2)
		{
			uint64_t trace_id[2];
			TRACE_ID_HEX_TO_BIN(value.data(), trace_id);
			data[SRPC_TRACE_ID] = std::string((char *)trace_id, SRPC_TRACEID_SIZE);
			flag |= 1;
		}
		else if (strcasecmp(name.c_str(), "Span-Id") == 0 &&
				 value.size() == SRPC_SPANID_SIZE * 2)
		{
			uint64_t span_id[1];
			SPAN_ID_HEX_TO_BIN(value.data(), span_id);
			data[SRPC_SPAN_ID] = std::string((char *)span_id, SRPC_SPANID_SIZE);
			flag |= (1 << 1);
		}
	}

	return true;
}

} // end namespace srpc

