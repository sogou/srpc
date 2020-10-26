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

#ifndef __RPC_SPAN_H__
#define __RPC_SPAN_H__

#include <time.h>
#include <atomic>
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "rpc_basic.h"

namespace srpc
{

class SnowFlake
{
public:
	SnowFlake() :
		SnowFlake(TIMESTAMP_BITS, GROUP_BITS, MACHINE_BITS)
	{
	}

	SnowFlake(uint64_t timestamp_bits,
			  uint64_t group_bits,
			  uint64_t machine_bits)
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

	bool get_uid(uint64_t group_id, uint64_t machine_id, uint64_t *uid)
	{
		if (group_id > this->machine_id_max || machine_id > this->machine_id_max)
			return false;

		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		uint64_t timestamp = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
		uint64_t seq_id;

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
	std::atomic<uint64_t> last_timestamp; // -1L;
	std::atomic<uint64_t> sequence; // 0L;

	uint64_t timestamp_bits;
	uint64_t group_bits;
	uint64_t machine_bits;
	uint64_t sequence_bits;

	uint64_t group_id_max;
	uint64_t machine_id_max;
	uint64_t sequence_max;

	uint64_t timestamp_shift;
	uint64_t group_shift;
	uint64_t machine_shift;

	// u_id = [timestamp][group][machine][sequence]
	static constexpr uint64_t TIMESTAMP_BITS = 37L;
	static constexpr uint64_t GROUP_BITS = 5L;
	static constexpr uint64_t MACHINE_BITS = 10L;
//	static constexpr uint64_t SEQUENCE_BITS = 12L;
	static constexpr uint64_t TOTAL_BITS = 64L;
};

class RPCSpan
{
private:
	uint64_t trace_id;
	uint32_t span_id;
	uint32_t parent_span_id;
	std::string service_name;
	std::string method_name;
	int data_type;
	int compress_type;
	uint64_t start_time;
	uint64_t end_time;
	uint64_t cost;
	std::string remote_ip;
	int status;
	int error;

public:
	RPCSpan() :
		trace_id(UINT64_UNSET),
		span_id(UINT_UNSET),
		parent_span_id(UINT_UNSET),
		service_name(""),
		method_name(""),
		data_type(INT_UNSET),
		compress_type(INT_UNSET),
		start_time(UINT64_UNSET),
		end_time(UINT64_UNSET),
		cost(UINT64_UNSET),
		remote_ip("")
	{}

	uint64_t get_trace_id() { return this->trace_id; }
	void set_trace_id(uint64_t id) { this->trace_id = id; }

	uint32_t get_span_id() { return this->span_id; }
	void set_span_id(uint32_t id) { this->span_id = id; }

	uint32_t get_parent_span_id() { return this->parent_span_id; }
	void set_parent_span_id(uint32_t id) { this->parent_span_id = id; }

	const std::string& get_service_name() { return this->service_name; }
	void set_service_name(std::string name) { this->service_name = name; }

	const std::string& get_method_name() { return this->method_name; }
	void set_method_name(std::string name) { this->method_name = name; }

	int get_data_type() { return this->data_type; }
	void set_data_type(int type) { this->data_type = type; }

	int get_compress_type() { return this->compress_type; }
	void set_compress_type(int type) { this->compress_type = type; }

	uint64_t get_start_time() { return this->start_time; }
	void set_start_time(uint64_t time) { this->start_time = time; }

	uint64_t get_end_time() { return this->end_time; }
	void set_end_time(uint64_t time) { this->end_time = time; }

	uint64_t get_cost() { return this->cost; }
	void set_cost(uint64_t time) { this->cost = time; }

	const std::string& get_remote_ip() { return this->remote_ip; }
	void set_remote_ip(std::string ip) { this->remote_ip = std::move(ip); }

	int get_status() { return this->status; }
	void set_status(int stat) { this->status = stat; }

	int get_error() { return this->error; }
	void set_error(int err) { this->error = err; }
};

class RPCSpanLogger
{
public:
	virtual SubTask* create_log_task(RPCSpan *span)
	{
		delete span;
		return WFTaskFactory::create_empty_task();
	}

	virtual RPCSpan *create_span()
	{
		return new RPCSpan();
	}
};

class RPCSeriesWork : public SeriesWork
{
public:
	RPCSeriesWork(SubTask *first, series_callback_t&& callback) :
		SeriesWork(first, std::move(callback)),
		span(NULL)
	{}

	RPCSeriesWork(SubTask *first, SubTask *last, series_callback_t&& callback) :
		SeriesWork(first, std::move(callback)),
		span(NULL)
	{
		this->set_last_task(last);
	}

	RPCSpan *get_span() const { return this->span; }
	void set_span(RPCSpan *span) { this->span = span; }
	bool has_span() const { return !!this->span; }
	void clear_span() { this->span = NULL; }

private:
	RPCSpan *span;
};

} // end namespace srpc

#endif
