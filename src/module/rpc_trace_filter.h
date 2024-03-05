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

#ifndef __RPC_TRACE_FILTER_H__
#define __RPC_TRACE_FILTER_H__

#include <time.h>
#include <mutex>
#include <atomic>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits>
#include <unordered_map>
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/RedisMessage.h"
#include "rpc_basic.h"
#include "rpc_trace_module.h"

namespace srpc
{

static constexpr unsigned int	SPAN_LIMIT_DEFAULT			= 1;
static constexpr size_t			SPAN_LOG_MAX_LENGTH			= 1024;
static constexpr size_t			UINT64_STRING_LENGTH		= 20;
static constexpr unsigned int	SPAN_REDIS_RETRY_MAX		= 0;
static constexpr const char	   *SPAN_BATCH_LOG_NAME_DEFAULT	= "./trace_info.log";
static constexpr size_t			SPAN_BATCH_LOG_SIZE_DEFAULT	= 4 * 1024 * 1024;
static constexpr unsigned int	SPANS_PER_SECOND_DEFAULT	= 1000;
static constexpr const char	   *OTLP_TRACES_PATH			= "/v1/traces";

class RPCTraceFilterPolicy
{
public:
	RPCTraceFilterPolicy(size_t spans_per_second,
						 size_t report_threshold,
						 size_t report_interval_msec) :
		stat_interval(1), // default 1 msec
		spans_per_sec(spans_per_second),
		last_collect_timestamp(0),
		spans_second_count(0),
		spans_interval_count(0),
		report_threshold(report_threshold),
		report_interval(report_interval_msec),
		last_report_timestamp(0)
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
		this->spans_per_interval = (this->spans_per_sec * msec + 999) / 1000;
	}

	void set_report_threshold(size_t threshold)
	{
		if (threshold <= 0)
			threshold = 1;
		this->report_threshold = threshold;
	}

	void set_report_interval(int msec)
	{
		if (msec <= 0)
			msec = 1;
		this->report_interval = msec;
	}

	bool collect(RPCModuleData& span);
	bool report(size_t count);

private:
	int stat_interval;
	size_t spans_per_sec;
	size_t spans_per_interval;
	long long last_collect_timestamp;
	std::atomic<size_t> spans_second_count;
	std::atomic<size_t> spans_interval_count;
	size_t report_threshold; // spans to report at most
	size_t report_interval;
	long long last_report_timestamp;
};

class RPCTraceLogTask : public WFGenericTask
{
public:
	RPCTraceLogTask(RPCModuleData& span,
				   std::function<void (RPCTraceLogTask *)> callback) :
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
	RPCModuleData span;
	std::function<void (RPCTraceLogTask *)> callback;
};

class RPCTraceDefault : public RPCFilter
{
public:
	RPCTraceDefault() :
		RPCFilter(RPCModuleTypeTrace),
		filter_policy(SPANS_PER_SECOND_DEFAULT,
					  RPC_REPORT_THREHOLD_DEFAULT,
					  RPC_REPORT_INTERVAL_DEFAULT)
	{}

	RPCTraceDefault(size_t spans_per_second) :
		RPCFilter(RPCModuleTypeTrace),
		filter_policy(spans_per_second,
					  RPC_REPORT_THREHOLD_DEFAULT,
					  RPC_REPORT_INTERVAL_DEFAULT)
	{}

private:
	SubTask *create(RPCModuleData& span) override
	{
		return new RPCTraceLogTask(span, nullptr);
	}

	bool filter(RPCModuleData& span) override
	{
		return this->filter_policy.collect(span);
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
	RPCTraceFilterPolicy filter_policy;
};

class RPCTraceRedis : public RPCFilter
{
public:
	RPCTraceRedis(const std::string& url) :
		RPCFilter(RPCModuleTypeTrace),
		retry_max(SPAN_REDIS_RETRY_MAX),
		filter_policy(SPANS_PER_SECOND_DEFAULT,
					  RPC_REPORT_THREHOLD_DEFAULT,
					  RPC_REPORT_INTERVAL_DEFAULT)
	{}

	RPCTraceRedis(const std::string& url, int retry_max,
				  size_t spans_per_second) :
		RPCFilter(RPCModuleTypeTrace),
		redis_url(url),
		retry_max(retry_max),
		filter_policy(spans_per_second,
					  RPC_REPORT_THREHOLD_DEFAULT,
					  RPC_REPORT_INTERVAL_DEFAULT)
	{}

private:
	std::string redis_url;
	int retry_max;

private:
	SubTask *create(RPCModuleData& span) override;

	bool filter(RPCModuleData& span) override
	{
		return this->filter_policy.collect(span);
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
	RPCTraceFilterPolicy filter_policy;
};

class RPCTraceOpenTelemetry : public RPCFilter
{
public:
	void set_spans_per_sec(size_t n)
	{
		this->filter_policy.set_spans_per_sec(n);
	}

	void set_stat_interval(int msec)
	{
		this->filter_policy.set_stat_interval(msec);
	}

	void set_report_threshold(size_t threshold)
	{
		this->filter_policy.set_report_threshold(threshold);
	}

	void set_report_interval(int msec)
	{
		this->filter_policy.set_report_interval(msec);
	}

	// attributes for client level, such as ProviderID
	void add_attributes(const std::string& key, const std::string& value);
	size_t clear_attributes();

	// attributes for each span
	void add_span_attributes(const std::string& key, const std::string& value);
	size_t clear_span_attributes();

private:
	std::string url;
	int redirect_max;
	int retry_max;

	RPCTraceFilterPolicy filter_policy;
	google::protobuf::Message *report_req;
	std::unordered_map<std::string, google::protobuf::Message *> report_map;
	bool report_status;
	size_t report_span_count;

protected:
	std::mutex mutex;
	std::unordered_map<std::string, std::string> attributes;
	std::unordered_map<std::string, std::string> span_attributes;

private:
	SubTask *create(RPCModuleData& span) override;
	bool filter(RPCModuleData& span) override;

public:
	RPCTraceOpenTelemetry(const std::string& url);

	RPCTraceOpenTelemetry(const std::string& url, const std::string& path);

	RPCTraceOpenTelemetry(const std::string& url,
						  const std::string& path,
						  int redirect_max,
						  int retry_max,
						  size_t spans_per_second,
						  size_t report_threshold,
						  size_t report_interval);

	virtual ~RPCTraceOpenTelemetry();
};

} // end namespace srpc

#endif

