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
#include <list>
#include "workflow/WFTask.h" 
#include "rpc_basic.h"
#include "rpc_filter.h"

namespace srpc
{

static constexpr char const *SRPC_COMPONENT_SRPC	= "srpc";

static constexpr char const *SRPC_SPAN_LOG			= "srpc.log";
static constexpr char const *SRPC_SPAN_EVENT		= "event";
static constexpr char const *SRPC_SPAN_MESSAGE		= "message";

static constexpr char const *SRPC_START_TIMESTAMP	= "srpc.start_time";
static constexpr char const *SRPC_FINISH_TIMESTAMP	= "srpc.finish_time";
static constexpr char const *SRPC_DURATION			= "srpc.duration";
static constexpr char const *SRPC_TIMEOUT_REASON	= "srpc.timeout_reason";

static constexpr char const *SRPC_SPAN_ID			= "srpc.span_id";
static constexpr char const *SRPC_TRACE_ID			= "srpc.trace_id";
static constexpr char const *SRPC_PARENT_SPAN_ID	= "srpc.parent_span_id";

//for SnowFlake: u_id = [timestamp][group][machine][sequence]
static constexpr int SRPC_TIMESTAMP_BITS		= 38;
static constexpr int SRPC_GROUP_BITS			= 4;
static constexpr int SRPC_MACHINE_BITS			= 10;
//static constexpr int SRPC_SEQUENCE_BITS		= 12;
static constexpr int SRPC_TOTAL_BITS			= 64;

class RPCModule
{
protected:
	virtual bool client_begin(SubTask *task, RPCModuleData& data) = 0;
	virtual bool server_begin(SubTask *task, RPCModuleData& data) = 0;
	virtual bool client_end(SubTask *task, RPCModuleData& data) = 0;
	virtual bool server_end(SubTask *task, RPCModuleData& data) = 0;

public:
	void add_filter(RPCFilter *filter)
	{
		this->mutex.lock();
		this->filters.push_back(filter);
		this->mutex.unlock();
	}

	size_t get_filters_size() const { return this->filters.size(); }
	enum RPCModuleType get_module_type() const { return this->module_type; }

	bool client_task_begin(SubTask *task, RPCModuleData& data);
	bool server_task_begin(SubTask *task, RPCModuleData& data);
	bool client_task_end(SubTask *task, RPCModuleData& data);
	bool server_task_end(SubTask *task, RPCModuleData& data);

	RPCModule(enum RPCModuleType module_type)
	{
		this->module_type = module_type;
	}
	virtual ~RPCModule() {}

private:
	enum RPCModuleType module_type;
	std::mutex mutex;
	std::list<RPCFilter *> filters;
};

class SnowFlake
{
public:
	SnowFlake(int timestamp_bits, int group_bits, int machine_bits);

	SnowFlake() :
		SnowFlake(SRPC_TIMESTAMP_BITS, SRPC_GROUP_BITS, SRPC_MACHINE_BITS)
	{
	}

	bool get_id(long long group_id, long long machine_id,
				unsigned long long *uid);

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
};

class CustomModule : public RPCModule
{
public:
	bool client_begin(SubTask *task, RPCModuleData& data) override;
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_begin(SubTask *task, RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;

public:
	CustomModule() : RPCModule(RPCModuleTypeCustom) { }
};

template<class SERVER_TASK, class CLIENT_TASK>
class RPCCustomModule : public CustomModule
{
};

bool http_get_header_module_data(const protocol::HttpMessage *msg,
								 RPCModuleData& data);

bool http_set_header_module_data(const RPCModuleData& data,
								 protocol::HttpMessage *msg);

} // end namespace srpc

#endif

