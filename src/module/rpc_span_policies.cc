#include <stdio.h>
#include <limits.h>
#include "workflow/WFTask.h"
#include "workflow/HttpUtil.h"
#include "rpc_span_policies.h"
#include "opentelemetry_trace_service.pb.h"

namespace srpc
{

using namespace opentelemetry::proto::collector::trace::v1;
using namespace opentelemetry::proto::trace::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

static size_t rpc_span_pb_format(RPCModuleData& data,
	const std::unordered_map<std::string, std::string>& attributes,
	ExportTraceServiceRequest& req)
{

	ResourceSpans *resource_span = req.add_resource_spans();
	InstrumentationLibrarySpans *ins_lib;
	ins_lib = resource_span->add_instrumentation_library_spans();
	Resource *resource = resource_span->mutable_resource();

	for (const auto& attr : attributes)
	{
		KeyValue *attribute = resource->add_attributes();
		attribute->set_key(attr.first);
		AnyValue *value = attribute->mutable_value();
		value->set_string_value(attr.second);
	}

	Span *span = ins_lib->add_spans();

	KeyValue *attribute = span->add_attributes();
	attribute->set_key(SRPC_SERVICE_NAME);
	AnyValue *value = attribute->mutable_value();
	value->set_string_value(data[SRPC_SERVICE_NAME]);

	span->set_span_id(data[SRPC_SPAN_ID].c_str(), SRPC_SPANID_SIZE);
	span->set_trace_id(data[SRPC_TRACE_ID].c_str(), SRPC_TRACEID_SIZE);
	span->set_name(data[SRPC_METHOD_NAME]);

	for (const auto& iter : data)
	{
		if (iter.first.compare(SRPC_PARENT_SPAN_ID) == 0)
			span->set_parent_span_id(iter.second);
		else if (iter.first.compare(SRPC_SPAN_KIND) == 0)
		{
			if (iter.second.compare(SRPC_SPAN_KIND_CLIENT) == 0)
				span->set_kind(Span_SpanKind_SPAN_KIND_CLIENT);
			else if (iter.second.compare(SRPC_SPAN_KIND_SERVER) == 0)
				span->set_kind(Span_SpanKind_SPAN_KIND_SERVER);
		}
		else if (iter.first.compare(SRPC_START_TIMESTAMP) == 0)
		{
			span->set_start_time_unix_nano(
						atoll(data[SRPC_START_TIMESTAMP].c_str()) * 1000000);
		}
		else if (iter.first.compare(SRPC_FINISH_TIMESTAMP) == 0)
		{
			span->set_end_time_unix_nano(
						atoll(data[SRPC_FINISH_TIMESTAMP].c_str()) * 1000000);
		}
	}

	return req.ByteSize();
}

static size_t rpc_span_log_format(RPCModuleData& data, char *str, size_t len)
{
	size_t ret = snprintf(str, len, "trace_id: %s span_id: %s service: %s"
									" method: %s start_time: %s",
						  data[SRPC_TRACE_ID].c_str(),
						  data[SRPC_SPAN_ID].c_str(),
						  data[SRPC_SERVICE_NAME].c_str(),
						  data[SRPC_METHOD_NAME].c_str(),
						  data[SRPC_START_TIMESTAMP].c_str());

	auto iter = data.find(SRPC_PARENT_SPAN_ID);
	if (iter != data.end())
	{
		ret += snprintf(str + ret, len - ret, " parent_span_id: %s",
						iter->second.c_str());
	}

	iter = data.find(SRPC_FINISH_TIMESTAMP);
	if (iter != data.end())
	{
		ret += snprintf(str + ret, len - ret, " finish_time: %s",
						iter->second.c_str());
	}

	iter = data.find(SRPC_DURATION);
	if (iter != data.end())
	{
		ret += snprintf(str + ret, len - ret, " duration: %s"
											  " remote_ip: %s port: %s"
											  " state: %s error: %s",
						iter->second.c_str(),
						data[SRPC_REMOTE_IP].c_str(),
						data[SRPC_REMOTE_PORT].c_str(),
						data[SRPC_STATE].c_str(),
						data[SRPC_ERROR].c_str());
	}

	for (const auto& it : data)
	{
		if (strncmp(it.first.c_str(), SRPC_SPAN_LOG, 3) == 0)
			ret += snprintf(str + ret, len - ret,
							"\n%s trace_id: %s span_id: %s"
							" timestamp: %s %s",
							"[ANNOTATION]",
							data[SRPC_TRACE_ID].c_str(),
							data[SRPC_SPAN_ID].c_str(),
							it.first.c_str() + 4,
							it.second.c_str());
	}

	return ret;
}

bool RPCSpanFilterPolicy::filter(RPCModuleData& span)
{
	long long timestamp = GET_CURRENT_MS();

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
	auto iter = span.find(SRPC_TRACE_ID);
	if (iter == span.end())
		return WFTaskFactory::create_empty_task();

	auto *task = WFTaskFactory::create_redis_task(this->redis_url,
												  this->retry_max,
												  nullptr);
	protocol::RedisRequest *req = task->get_req();
	char value[SPAN_LOG_MAX_LENGTH];
	value[0] = '0';

	rpc_span_log_format(span, value, SPAN_LOG_MAX_LENGTH);
	req->set_request("SET", { span[SRPC_TRACE_ID], value} );

	return task;
}

SubTask *RPCSpanOpenTelemetry::create(RPCModuleData& span)
{
	auto iter = span.find(SRPC_TRACE_ID);
	if (iter == span.end())
		return WFTaskFactory::create_empty_task();

	ExportTraceServiceRequest req;
	this->mutex.lock();
	rpc_span_pb_format(span, this->attributes, req);
	this->mutex.unlock();

	WFHttpTask *task = WFTaskFactory::create_http_task(this->url,
													   this->redirect_max,
													   this->retry_max,
													   [](WFHttpTask *task) {
		delete (std::string *)task->user_data;
	});

	protocol::HttpRequest *http_req = task->get_req();
	http_req->set_method(HttpMethodPost);
	http_req->add_header_pair("Content-Type", "application/x-protobuf");

	task->user_data = new std::string;	
	std::string *output = (std::string *)task->user_data;
	req.SerializeToString(output);
	http_req->append_output_body_nocopy(output->c_str(), output->length());

	return task;
}

void RPCSpanOpenTelemetry::add_attributes(const std::string& key,
										  const std::string& value)
{
	this->mutex.lock();
	this->attributes.insert(std::make_pair(key, value));
	this->mutex.unlock();
}

size_t RPCSpanOpenTelemetry::clear_attributes()
{
	size_t ret;

	this->mutex.lock();
	ret = this->attributes.size();
	this->attributes.clear();
	this->mutex.unlock();

	return ret;
}

}

