#include <stdio.h>
#include <limits.h>
#include "workflow/WFTask.h"
#include "workflow/HttpUtil.h"
#include "rpc_filter_span.h"
#include "opentelemetry_trace_service.pb.h"

namespace srpc
{

using namespace opentelemetry::proto::collector::trace::v1;
using namespace opentelemetry::proto::trace::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

static InstrumentationLibrarySpans *
rpc_span_fill_pb_request(const std::string& service_name,
		const std::unordered_map<std::string, std::string>& attributes,
		ExportTraceServiceRequest *req)
{
	ResourceSpans *rs = req->add_resource_spans();
	InstrumentationLibrarySpans *spans = rs->add_instrumentation_library_spans();
	Resource *resource = rs->mutable_resource();

	for (const auto& attr : attributes)
	{
		KeyValue *attribute = resource->add_attributes();
		attribute->set_key(attr.first);
		AnyValue *value = attribute->mutable_value();
		value->set_string_value(attr.second);
	}

	KeyValue *attribute = resource->add_attributes();
	attribute->set_key("service.name");
	AnyValue *value = attribute->mutable_value();
	value->set_string_value(service_name);

	return spans;
}

static void rpc_span_fill_pb_span(RPCModuleData& data,
								  InstrumentationLibrarySpans *spans)
{
	Span *span = spans->add_spans();

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
						1000000 * atoll(data[SRPC_START_TIMESTAMP].c_str()));
		}
		else if (iter.first.compare(SRPC_FINISH_TIMESTAMP) == 0)
		{
			span->set_end_time_unix_nano(
						1000000 * atoll(data[SRPC_FINISH_TIMESTAMP].c_str()));
		}
	}
}

static size_t rpc_span_log_format(RPCModuleData& data, char *str, size_t len)
{
	const uint64_t *trace_id = (const uint64_t *)data[SRPC_TRACE_ID].c_str();
	const uint64_t *span_id = (const uint64_t *)data[SRPC_SPAN_ID].c_str();
	char trace_id_buf[SRPC_TRACEID_SIZE * 2 + 1];
	char span_id_buf[SRPC_SPANID_SIZE * 2 + 1];

	TRACE_ID_BIN_TO_HEX(trace_id, trace_id_buf);
	SPAN_ID_BIN_TO_HEX(span_id, span_id_buf);

	size_t ret = snprintf(str, len, "trace_id: %s span_id: %s service: %s"
									" method: %s start_time: %s",
						  trace_id_buf,
						  span_id_buf,
						  data[SRPC_SERVICE_NAME].c_str(),
						  data[SRPC_METHOD_NAME].c_str(),
						  data[SRPC_START_TIMESTAMP].c_str());

	auto iter = data.find(SRPC_PARENT_SPAN_ID);
	if (iter != data.end())
	{
		char parent_span_id_buf[SRPC_SPANID_SIZE * 2 + 1];
		span_id = (const uint64_t *)iter->second.c_str();

		SPAN_ID_BIN_TO_HEX(span_id, parent_span_id_buf);
		ret += snprintf(str + ret, len - ret, " parent_span_id: %s",
						parent_span_id_buf);
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
		if (strncmp(it.first.c_str(), SRPC_SPAN_LOG, strlen(SRPC_SPAN_LOG)) == 0)
			ret += snprintf(str + ret, len - ret,
							"\n%s trace_id: %s span_id: %s"
							" timestamp: %s %s",
							"[ANNOTATION]",
							trace_id_buf,
							span_id_buf,
							it.first.c_str() + strlen(SRPC_SPAN_LOG) + 1,
							it.second.c_str());
	}

	return ret;
}

bool RPCSpanFilterPolicy::collect(RPCModuleData& span)
{
	long long timestamp = GET_CURRENT_MS();

	if (timestamp < this->last_collect_timestamp + this->stat_interval &&
		this->spans_interval_count < this->spans_per_interval &&
		this->spans_second_count < this->spans_per_sec)
	{
		this->spans_interval_count++;
		this->spans_second_count++;
		return true;
	}
	else if (timestamp >= this->last_collect_timestamp + this->stat_interval &&
			this->spans_per_sec)
	{
		this->spans_interval_count = 0;

		if (timestamp / 1000 > this->last_collect_timestamp / 1000) // next second
			this->spans_second_count = 0;

		this->last_collect_timestamp = timestamp;
		if (this->spans_second_count < this->spans_per_sec)
		{
			this->spans_second_count++;
			this->spans_interval_count++;
			return true;
		}
	}

	return false;
}

bool RPCSpanFilterPolicy::report(size_t count)
{
	long long timestamp = GET_CURRENT_MS();

	if (this->last_report_timestamp == 0)
		this->last_report_timestamp = timestamp;

	if (timestamp > this->last_report_timestamp + (long long)this->report_interval ||
		count >= this->report_threshold)
	{
		this->last_report_timestamp = timestamp;
		return true;
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

RPCSpanOpenTelemetry::RPCSpanOpenTelemetry(const std::string& url) :
	RPCFilter(RPCModuleSpan),
	url(url + SPAN_OTLP_TRACES_PATH),
	redirect_max(SPAN_HTTP_REDIRECT_MAX),
	retry_max(SPAN_HTTP_RETRY_MAX),
	filter_policy(SPANS_PER_SECOND_DEFAULT,
				  REPORT_THREHOLD_DEFAULT,
				  REPORT_INTERVAL_DEFAULT),
	report_status(false),
	report_span_count(0)
{
	this->report_req = new ExportTraceServiceRequest;
}

RPCSpanOpenTelemetry::RPCSpanOpenTelemetry(const std::string& url,
										   unsigned int redirect_max,
										   unsigned int retry_max,
										   size_t spans_per_second,
										   size_t report_threshold,
										   size_t report_interval) :
	RPCFilter(RPCModuleSpan),
	url(url + SPAN_OTLP_TRACES_PATH),
	redirect_max(redirect_max),
	retry_max(retry_max),
	filter_policy(spans_per_second, report_threshold, report_interval),
	report_status(false),
	report_span_count(0)
{
	this->report_req = new ExportTraceServiceRequest;
}

RPCSpanOpenTelemetry::~RPCSpanOpenTelemetry()
{
	delete this->report_req;
}

SubTask *RPCSpanOpenTelemetry::create(RPCModuleData& span)
{
	std::string *output = new std::string;
	SubTask *next = NULL;
	ExportTraceServiceRequest *req = (ExportTraceServiceRequest *)this->report_req;

	this->mutex.lock();
	if (!this->report_status)
		next = WFTaskFactory::create_empty_task();
	else
	{
		req->SerializeToString(output);
		this->report_status = false;
		this->report_span_count = 0;
		req->clear_resource_spans();
		this->report_map.clear();
	}
	this->mutex.unlock();

	if (next)
		return next;

	WFHttpTask *task = WFTaskFactory::create_http_task(this->url,
													   this->redirect_max,
													   this->retry_max,
													   [](WFHttpTask *task) {
		delete (std::string *)task->user_data;
	});

	protocol::HttpRequest *http_req = task->get_req();
	http_req->set_method(HttpMethodPost);
	http_req->add_header_pair("Content-Type", "application/x-protobuf");

	task->user_data = output;
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

bool RPCSpanOpenTelemetry::filter(RPCModuleData& data)
{
	std::unordered_map<std::string, google::protobuf::Message *>::iterator it;
	InstrumentationLibrarySpans *spans;
	bool ret;
	const std::string& service_name = data[SRPC_SERVICE_NAME];

	this->mutex.lock();
	if (this->filter_policy.collect(data))
	{
		++this->report_span_count;
		it = report_map.find(service_name);
		if (it == report_map.end())
		{
			spans = rpc_span_fill_pb_request(service_name, this->attributes,
							(ExportTraceServiceRequest *)this->report_req);
			this->report_map.insert({service_name, spans});
		}
		else
			spans = (InstrumentationLibrarySpans *)it->second;

		rpc_span_fill_pb_span(data, spans);
	}

	ret = this->filter_policy.report(this->report_span_count);
	if (ret)
		this->report_status = true;
	this->mutex.unlock();

	return ret;
}

}

