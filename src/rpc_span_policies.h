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
#include <unistd.h>
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

class RPCSpanFilterLogger : public RPCSpanLogger
{
public:
	SubTask* create_log_task(RPCSpan *span)
	{
		if (this->filter(span))
			return this->create(span);

		delete span;
		return WFTaskFactory::create_empty_task();
	}

	RPCSpanFilterLogger() :
		span_limit(SPAN_LIMIT_DEFAULT),
		span_timestamp(0L),
		span_count(0)
	{
	}

	void set_span_limit(unsigned int limit) { this->span_limit = limit; }

private:
	virtual SubTask *create(RPCSpan *span) = 0;

	virtual bool filter(RPCSpan *span)
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		uint64_t timestamp = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

		if ((timestamp == this->span_timestamp &&
					this->span_count < this->span_limit) ||
			span->get_trace_id() != UINT64_UNSET)
		{
			this->span_count++;
		}
		else if (timestamp > this->span_timestamp)
		{
			this->span_count = 0;
			this->span_timestamp = timestamp;
		} else
			return false;

		return true;
	}

private:
	unsigned int span_limit;
	uint64_t span_timestamp;
	std::atomic<unsigned int> span_count;
};

static size_t rpc_span_log_format(RPCSpan *span, char *str, size_t len)
{
	size_t ret = snprintf(str, len, "trace_id: %lu span_id: %lu service: %s"
						 			" method: %s start: %lu",
					  	 span->get_trace_id(),
						 span->get_span_id(),
					  	 span->get_service_name().c_str(),
						 span->get_method_name().c_str(),
					  	 span->get_start_time());

	if (span->get_parent_span_id() != UINT64_UNSET)
	{
		ret += snprintf(str + ret, len - ret, " parent_span_id: %lu",
						span->get_parent_span_id());
	}
	if (span->get_end_time() != UINT64_UNSET)
	{
		ret += snprintf(str + ret, len - ret, " end_time: %lu",
						span->get_end_time());
	}
	if (span->get_cost() != UINT64_UNSET)
	{
		ret += snprintf(str + ret, len - ret, " cost: %lu remote_ip: %s"
											  " state: %d error: %d",
						span->get_cost(), span->get_remote_ip().c_str(),
						span->get_state(), span->get_error());
	}


	return ret;
}

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
private:
	SubTask *create(RPCSpan *span)
	{
		return new RPCSpanLogTask(span, [span](RPCSpanLogTask *task) {
											delete span;
										});
	}
};

class RPCSpanBatchLogger : public RPCSpanFilterLogger
{
public:
	RPCSpanBatchLogger() :
		RPCSpanBatchLogger(SPAN_BATCH_LOG_NAME_DEFAULT,
						   SPAN_BATCH_LOG_SIZE_DEFAULT)
	{}

	RPCSpanBatchLogger(const char *log_name, size_t buffer_size) :
		buffer_size(buffer_size),
		offset(0)
	{
		if (this->buffer_size < SPAN_LOG_MAX_LENGTH)
			this->buffer_size = SPAN_LOG_MAX_LENGTH;

		this->buffer = (char *)malloc(this->buffer_size);
		this->pos = this->buffer;
		this->fd = open(log_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
		this->offset = lseek(this->fd, 0, SEEK_END);
	}

	~RPCSpanBatchLogger()
	{
		if (this->fd > 0 && this->pos != this->buffer)
			write(this->fd, this->buffer, this->pos - this->buffer);

		free(this->buffer);
		close(this->fd);
	}

	SubTask *create(RPCSpan *span)
	{
		do
		{
			if (this->fd < 0)
				break;

			mutex.lock();

			size_t len = this->pos - this->buffer;
			len = rpc_span_log_format(span, this->pos, this->buffer_size - len);
			if (len > SPAN_LOG_MAX_LENGTH)
				break;

			this->pos += len;
			*this->pos = '\n';
			len++;
			this->pos++;

			len = this->pos - this->buffer;

			char *buf = NULL;
			if (len + SPAN_LOG_MAX_LENGTH >= this->buffer_size) // = for '\n'
			{
				buf = this->buffer;
				this->buffer = (char *)malloc(len);
				this->offset += len;
				this->pos = this->buffer;
			}
			mutex.unlock();

			if (!buf)
				break;

			delete span;
			return WFTaskFactory::create_pwrite_task(this->fd,
													 buf, len,
													 this->offset,
													 [buf](WFFileIOTask *task) {
													 	free(buf);
													 });
		} while (0);

		delete span;
		return WFTaskFactory::create_empty_task();
	}

private:
	size_t buffer_size;
	char *buffer;
	size_t offset;
	char *pos;
	int fd;
	std::mutex mutex;
};

class RPCSpanRedisLogger : public RPCSpanFilterLogger
{
public:
	RPCSpanRedisLogger() :
		RPCSpanRedisLogger(this->redis_url, SPAN_REDIS_RETRY_MAX)
	{}

	RPCSpanRedisLogger(const std::string& url, int retry_max) :
		redis_url(url),
		retry_max(retry_max)
	{}

	void set_redis_url(const std::string& url) { this->redis_url = url; }

private:
	std::string redis_url;
	int retry_max;

private:
	SubTask *create(RPCSpan *span)
	{
		auto *task = WFTaskFactory::create_redis_task(this->redis_url,
													  this->retry_max,
													  nullptr);

		protocol::RedisRequest *req = task->get_req();
		char key[UINT64_STRING_LENGTH] = { 0 };
		char value[SPAN_LOG_MAX_LENGTH] = { 0 };

		sprintf(key, "%llu", span->get_trace_id());
		rpc_span_log_format(span, value, SPAN_LOG_MAX_LENGTH);
		req->set_request("SET", { key, value} );
		delete span;

		return task;
	}
};

} // end namespace srpc

#endif

