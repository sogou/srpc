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
#include "rpc_module_span.h"
#include "rpc_basic.h"

namespace srpc
{

static const unsigned int	SPAN_LIMIT_DEFAULT			= 1;
static const size_t			SPAN_LOG_MAX_LENGTH			= 1024;
static const size_t			UINT64_STRING_LENGTH		= 20;
static const unsigned int	SPAN_REDIS_RETRY_MAX		= 0;
static const char		   *SPAN_BATCH_LOG_NAME_DEFAULT	= "./span_info.log";
static const size_t			SPAN_BATCH_LOG_SIZE_DEFAULT	= 4 * 1024 * 1024;
static const size_t			SPANS_PER_SECOND_DEFAULT	= 1000;
static const char		   *SRPC_FILTER_SPAN_DEFAULT	= "FilterSpanDefault";
static const char		   *SRPC_FILTER_SPAN_REDIS		= "FilterSpanRedis";

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
	RPCModuleData span;
	std::function<void (RPCSpanLogTask *)> callback;
};

class RPCSpanDefault : public RPCFilter
{
public:
	RPCSpanDefault() :
		RPCFilter(RPCModuleSpan, SRPC_FILTER_SPAN_DEFAULT),
		filter_policy(SPANS_PER_SECOND_DEFAULT) {}

	RPCSpanDefault(size_t spans_per_second) :
		RPCFilter(RPCModuleSpan, SRPC_FILTER_SPAN_DEFAULT),
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

class RPCSpanRedis : public RPCFilter
{
public:
	RPCSpanRedis(const std::string& url) :
		RPCFilter(RPCModuleSpan, SRPC_FILTER_SPAN_REDIS),
		retry_max(SPAN_REDIS_RETRY_MAX),
		filter_policy(SPANS_PER_SECOND_DEFAULT)
	{}

	RPCSpanRedis(const std::string& url, int retry_max,
				 size_t spans_per_second) :
		RPCFilter(RPCModuleSpan, SRPC_FILTER_SPAN_REDIS),
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

