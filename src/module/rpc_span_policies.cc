#include <stdio.h>
#include <limits.h>
#include "workflow/WFTask.h"
#include "rpc_span_policies.h"

namespace srpc
{

static size_t rpc_span_log_format(RPCModuleData& data, char *str, size_t len)
{
	size_t ret = snprintf(str, len, "trace_id: %s span_id: %s service: %s"
									" method: %s start: %s",
						  data["trace_id"].c_str(),
						  data["span_id"].c_str(),
						  data["service_name"].c_str(),
						  data["method_name"].c_str(),
						  data["start_time"].c_str());

	auto iter = data.find("parent_span_id");
	if (iter != data.end())
	{
		ret += snprintf(str + ret, len - ret, " parent_span_id: %s",
						iter->second.c_str());
	}

	iter = data.find("end_time");
	if (iter != data.end())
	{
		ret += snprintf(str + ret, len - ret, " end_time: %s",
						iter->second.c_str());
	}

	iter = data.find("cost");
	if (iter != data.end())
	{
		ret += snprintf(str + ret, len - ret, " cost: %s remote_ip: %s"
											  " state: %s error: %s",
						iter->second.c_str(), data["remote_ip"].c_str(),
						data["state"].c_str(), data["error"].c_str());
	}

	return ret;
}

bool RPCSpanFilterPolicy::filter(RPCModuleData& span)
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
	rpc_span_log_format(this->span, str, SPAN_LOG_MAX_LENGTH);
	fprintf(stderr, "[SPAN_LOG] %s\n", str);

	this->subtask_done();
}

SubTask *RPCSpanRedis::create(RPCModuleData& span)
{
	auto *task = WFTaskFactory::create_redis_task(this->redis_url,
												  this->retry_max,
												  nullptr);
	protocol::RedisRequest *req = task->get_req();
	char value[SPAN_LOG_MAX_LENGTH];
	value[0] = '0';

	rpc_span_log_format(span, value, SPAN_LOG_MAX_LENGTH);
	req->set_request("SET", { span["trace_id"], value} );

	return task;
}

}
