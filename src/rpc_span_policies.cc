#include <stdio.h>
#include <limits.h>
#include "workflow/WFTask.h"
#include "rpc_span_policies.h"

namespace srpc
{

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

bool RPCSpanFilterPolicy::filter(RPCSpan *span)
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

void RPCSpanLogTask::dispatch()
{
	char str[SPAN_LOG_MAX_LENGTH];
	rpc_span_log_format(span, str, SPAN_LOG_MAX_LENGTH);
	fprintf(stderr, "[SPAN_LOG] %s\n", str);

	this->subtask_done();
}

SubTask *RPCSpanRedisLogger::create(RPCSpan *span)
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

}
