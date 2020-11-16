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
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/RedisMessage.h"
#include "rpc_span.h"

namespace srpc
{

static constexpr unsigned int	SPAN_LIMIT_DEFAULT				= 1;
static constexpr size_t			SPAN_LOG_MAX_LENGTH				= 1024;
static constexpr size_t			UINT64_STRING_LENGTH			= 20;
static constexpr unsigned int	SPAN_REDIS_RETRY_MAX			= 0;
static constexpr const char 	*SPAN_BATCH_LOG_NAME_DEFAULT	= "./span_info.log";
static constexpr size_t			SPAN_BATCH_LOG_SIZE_DEFAULT		= 4 * 1024 * 1024;
static constexpr size_t			SPANS_PER_SECOND_DEFAULT		= 1000;

class RPCSpanFilterLogger : public RPCSpanLogger
{
public:
	SubTask* create_log_task(RPCSpan *span) override
	{
		if (this->filter(span))
			return this->create(span);

		delete span;
		return WFTaskFactory::create_empty_task();
	}

private:
	virtual SubTask *create(RPCSpan *span) = 0;

	virtual bool filter(RPCSpan *span) = 0;
};

static size_t rpc_span_log_format(RPCSpan *span, char *str, size_t len)
{
	size_t ret = snprintf(str, len, "trace_id: %lld span_id: %lld service: %s"
						 			" method: %s start: %lld",
					  	 span->get_trace_id(),
						 span->get_span_id(),
					  	 span->get_service_name().c_str(),
						 span->get_method_name().c_str(),
					  	 span->get_start_time());

	if (span->get_parent_span_id() != LLONG_MAX)
	{
		ret += snprintf(str + ret, len - ret, " parent_span_id: %lld",
						span->get_parent_span_id());
	}
	if (span->get_end_time() != LLONG_MAX)
	{
		ret += snprintf(str + ret, len - ret, " end_time: %lld",
						span->get_end_time());
	}
	if (span->get_cost() != LLONG_MAX)
	{
		ret += snprintf(str + ret, len - ret, " cost: %lld remote_ip: %s"
											  " state: %d error: %d",
						span->get_cost(), span->get_remote_ip().c_str(),
						span->get_state(), span->get_error());
	}


	return ret;
}

class RPCSpanFilterPolicy
{
public:
	RPCSpanFilterPolicy(size_t spans_per_second) :
		spans_per_sec(spans_per_second),
		stat_interval(1), // default 1 msec
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

	bool filter(RPCSpan *span)
	{
		long long timestamp = GET_CURRENT_MS;

		if (timestamp < this->last_timestamp + this->stat_interval &&
			this->spans_interval_count < this->spans_per_interval &&
			this->spans_second_count < this->spans_per_sec)
		{
			this->spans_interval_count++;
			this->spans_second_count++;
			return true;
		}
		else if (timestamp >= this->last_timestamp + this->stat_interval &&
				this->spans_per_sec)
		{
			this->spans_interval_count = 0;

			if (timestamp / 1000 > this->last_timestamp / 1000) // next second
				this->spans_second_count = 0;

			this->last_timestamp = timestamp;
			if (this->spans_second_count < this->spans_per_sec)
			{
				this->spans_second_count++;
				this->spans_interval_count++;
				return true;
			}
		}

		return false;
	}

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
	RPCSpanLogTask(RPCSpan *span, std::function<void (RPCSpanLogTask *)> callback) :
		span(span),
		callback(std::move(callback))
	{}

private:
	virtual void dispatch()
	{
		char str[SPAN_LOG_MAX_LENGTH];
		rpc_span_log_format(span, str, SPAN_LOG_MAX_LENGTH);
		fprintf(stderr, "[SPAN_LOG] %s\n", str);

		this->subtask_done();
	}

	virtual SubTask *done()
	{
		SeriesWork *series = series_of(this);

		if (this->callback)
			this->callback(this);

		delete this;
		return series->pop();
	}
public:
	RPCSpan *span;
	std::function<void (RPCSpanLogTask *)> callback;
};

class RPCSpanLoggerDefault : public RPCSpanFilterLogger
{
public:
	RPCSpanLoggerDefault() :
		filter_policy(SPANS_PER_SECOND_DEFAULT) {}

	RPCSpanLoggerDefault(size_t spans_per_second) :
		filter_policy(spans_per_second) {}

private:
	SubTask *create(RPCSpan *span) override
	{
		return new RPCSpanLogTask(span, [span](RPCSpanLogTask *task) {
											delete span;
										});
	}

	bool filter(RPCSpan *span) override
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

class RPCSpanRedisLogger : public RPCSpanFilterLogger
{
public:
	RPCSpanRedisLogger(const std::string& url) :
		RPCSpanRedisLogger(url, SPAN_REDIS_RETRY_MAX,
						   SPANS_PER_SECOND_DEFAULT)
	{}

	RPCSpanRedisLogger(const std::string& url, int retry_max,
					   size_t spans_per_second) :
		filter_policy(spans_per_second),
		redis_url(url),
		retry_max(retry_max)
	{}

private:
	std::string redis_url;
	int retry_max;

private:
	SubTask *create(RPCSpan *span) override
	{
		auto *task = WFTaskFactory::create_redis_task(this->redis_url,
													  this->retry_max,
													  nullptr);

		protocol::RedisRequest *req = task->get_req();
		char key[UINT64_STRING_LENGTH];
		char value[SPAN_LOG_MAX_LENGTH];
		key[0] = '0';
		value[0] = '0';

		sprintf(key, "%lld", span->get_trace_id());
		rpc_span_log_format(span, value, SPAN_LOG_MAX_LENGTH);
		req->set_request("SET", { key, value} );
		delete span;

		return task;
	}

	bool filter(RPCSpan *span) override
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

