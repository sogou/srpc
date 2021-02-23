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

#ifndef __RPC_SPAN_POLICIES_H__
#define __RPC_SPAN_POLICIES_H__

#include <time.h>
#include <mutex>
#include <atomic>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits>
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/RedisMessage.h"
#include "rpc_module_impl.h"

namespace srpc
{

static constexpr unsigned int	SPAN_LIMIT_DEFAULT				= 1;
static constexpr size_t			SPAN_LOG_MAX_LENGTH				= 1024;
static constexpr size_t			UINT64_STRING_LENGTH			= 20;
static constexpr unsigned int	SPAN_REDIS_RETRY_MAX			= 0;
static constexpr const char 	*SPAN_BATCH_LOG_NAME_DEFAULT	= "./span_info.log";
static constexpr size_t			SPAN_BATCH_LOG_SIZE_DEFAULT		= 4 * 1024 * 1024;
static constexpr size_t			SPANS_PER_SECOND_DEFAULT		= 1000;

template<class RPCTYPE>
class RPCClientSpan : public RPCClientModule<RPCTYPE>
{
public:
	using TASK = RPCClientTask<typename RPCTYPE::REQ, typename RPCTYPE::RESP>;

	int begin(TASK *task, const RPCModuleData& data) override
	{
		auto *req = task->get_req();
		RPCModuleData& module_data = task->mutable_module_data();

		if (!data.empty())
		{
			//module_data["trace_id"] = data["trace_id"];
			auto iter = data.find("span_id");
			if (iter != data.end())
				module_data["parent_span_id"] = iter->second;
		} else {
			module_data["trace_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_trace_id());
		}

		module_data["span_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_span_id());

		module_data["service_name"] = req->get_service_name();
		module_data["method_name"] = req->get_method_name();
		module_data["data_type"] = std::to_string(req->get_data_type());
		module_data["compress_type"] = std::to_string(req->get_compress_type());
		module_data["start_time"] = std::to_string(GET_CURRENT_MS);

		return 0; // always success
	}

	//outside get data from resp->meta and call end()
	int end(TASK *task, const RPCModuleData& data) override
	{
		auto *resp = task->get_resp();
		RPCModuleData& module_data = task->mutable_module_data();
		long long end_time = GET_CURRENT_MS;

		module_data["end_time"] = std::to_string(end_time);
		module_data["cost"] = std::to_string(end_time -
											 atoll(module_data["start_time"].c_str()));
		module_data["state"] = std::to_string(resp->get_status_code());
		module_data["error"] = std::to_string(resp->get_error());
		module_data["remote_ip"] = task->get_remote_ip();

		// whether create a module task is depend on you and your derived class
		SubTask *module_task = this->create_module_task(module_data);
		series_of(task)->push_front(module_task);

		return 0;
	}
};

template<class RPCTYPE>
class RPCServerSpan : public RPCServerModule<RPCTYPE>
{
public:
	using TASK = RPCServerTask<typename RPCTYPE::REQ, typename RPCTYPE::RESP>;

	// outside will fill data from meta and call begin() and put data onto series
	int begin(TASK *task, const RPCModuleData& data) override
	{
		auto *req = task->get_req();
		RPCModuleData& module_data = task->mutable_module_data();

		module_data["service_name"] = req->get_service_name();
		module_data["method_name"] = req->get_method_name();
		module_data["data_type"] = std::to_string(req->get_data_type());
		module_data["compress_type"] = std::to_string(req->get_compress_type());

		if (module_data["trace_id"].empty())
			module_data["trace_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_trace_id());
		module_data["span_id"] = std::to_string(
								SRPCGlobal::get_instance()->get_span_id());
		module_data["start_time"] = std::to_string(GET_CURRENT_MS);

//		RPCSeriesWork *series = dynamic_cast<RPCSeriesWork *>(series_of(task));
//		if (series)
//			series->set_data(task_data);

		return 0;
	}

	int end(TASK *task, const RPCModuleData& data) override
	{
		auto *resp = task->get_resp();
		RPCModuleData& module_data = task->mutable_module_data();
		long long end_time = GET_CURRENT_MS;

		module_data["end_time"] = std::to_string(end_time);
		module_data["cost"] = std::to_string(end_time -
											 atoll(module_data["start_time"].c_str()));
		module_data["state"] = std::to_string(resp->get_status_code());
		module_data["error"] = std::to_string(resp->get_error());
		module_data["remote_ip"] = task->get_remote_ip();

		// whether create a module task is depend on you and your derived class
		SubTask *module_task = this->create_module_task(module_data);
		series_of(task)->push_front(module_task);

//		RPCSeriesWork *series = dynamic_cast<RPCSeriesWork *>(series_of(this));
//		if (series)
//			series->clear_span();

		return 0;
	}
};

class RPCSpanFilter
{
public:
	SubTask* create_span_task(RPCModuleData& span)
	{
		if (this->filter(span))
			return this->create(span);

		return WFTaskFactory::create_empty_task();
	}

private:
	virtual SubTask *create(RPCModuleData& span) = 0;

	virtual bool filter(RPCModuleData& span) = 0;
};

class RPCSpanFilterPolicy
{
public:
	RPCSpanFilterPolicy(size_t spans_per_second) :
		stat_interval(1), // default 1 msec
		spans_per_sec(spans_per_second),
		last_timestamp(0L),
		spans_second_count(0),
		spans_interval_count(0)
	{
		this->spans_per_interval = (this->spans_per_sec + 999) / 1000;
	}

	void set_spans_per_sec(size_t n)
	{
		this->spans_per_sec = n;
		this->spans_per_interval = (n * this->stat_interval + 999 ) / 1000;
	}

	void set_stat_interval(int msec)
	{
		if (msec <= 0)
			msec = 1;
		this->stat_interval = msec;
		this->spans_per_interval = (this->spans_per_sec * msec + 999 ) / 1000;
	}

	bool filter(RPCModuleData& span);

private:
	int stat_interval;
	size_t spans_per_sec;
	size_t spans_per_interval;
	std::atomic<long long> last_timestamp;
	std::atomic<size_t> spans_second_count;
	std::atomic<size_t> spans_interval_count;
};

class RPCSpanLogTask : public WFGenericTask
{
public:
	RPCSpanLogTask(RPCModuleData& span,
				   std::function<void (RPCSpanLogTask *)> callback) :
		span(span),
		callback(std::move(callback))
	{}

private:
	virtual void dispatch();

	virtual SubTask *done()
	{
		SeriesWork *series = series_of(this);

		if (this->callback)
			this->callback(this);

		delete this;
		return series->pop();
	}

public:
	RPCModuleData& span;
	std::function<void (RPCSpanLogTask *)> callback;
};

class RPCSpanDefault : public RPCSpanFilter
{
public:
	RPCSpanDefault() :
		filter_policy(SPANS_PER_SECOND_DEFAULT) {}

	RPCSpanDefault(size_t spans_per_second) :
		filter_policy(spans_per_second) {}

private:
	SubTask *create(RPCModuleData& span) override
	{
		return new RPCSpanLogTask(span, nullptr);
	}

	bool filter(RPCModuleData& span) override
	{
		return this->filter_policy.filter(span);
	}

public:
	void set_spans_per_sec(size_t n)
	{
		this->filter_policy.set_spans_per_sec(n);
	}

	void set_stat_interval(int msec)
	{
		this->filter_policy.set_stat_interval(msec);
	}

private:
	RPCSpanFilterPolicy filter_policy;
};

class RPCSpanRedis : public RPCSpanFilter
{
public:
	RPCSpanRedis(const std::string& url) :
		RPCSpanRedis(url, SPAN_REDIS_RETRY_MAX,
						   SPANS_PER_SECOND_DEFAULT)
	{}

	RPCSpanRedis(const std::string& url, int retry_max,
					   size_t spans_per_second) :
		redis_url(url),
		retry_max(retry_max),
		filter_policy(spans_per_second)
	{}

private:
	std::string redis_url;
	int retry_max;

private:
	SubTask *create(RPCModuleData& span) override;

	bool filter(RPCModuleData& span) override
	{
		return this->filter_policy.filter(span);
	}

public:
	void set_spans_per_sec(size_t n)
	{
		this->filter_policy.set_spans_per_sec(n);
	}

	void set_stat_interval(int msec)
	{
		this->filter_policy.set_stat_interval(msec);
	}

private:
	RPCSpanFilterPolicy filter_policy;
};

} // end namespace srpc

#endif

