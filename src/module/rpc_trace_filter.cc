/*
  Copyright (c) 2023 Sogou, Inc.

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

#include <stdio.h>
#include <limits.h>
#include "workflow/WFTask.h"
#include "workflow/HttpUtil.h"
#include "rpc_trace_filter.h"
#include "opentelemetry_trace.pb.h"

namespace srpc
{

using namespace opentelemetry::proto::trace::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

static constexpr char const *SRPC_COMPONENT_OTEL_STR = "rpc.system";

static InstrumentationLibrarySpans *
rpc_span_fill_pb_request(const RPCModuleData& data,
		const std::unordered_map<std::string, std::string>& attributes,
		TracesData *req)
{
	ResourceSpans *rs = req->add_resource_spans();
	InstrumentationLibrarySpans *spans = rs->add_instrumentation_library_spans();
	Resource *resource = rs->mutable_resource();
	KeyValue *attribute;
	AnyValue *value;

	auto iter = data.find(OTLP_METHOD_NAME);
	if (iter != data.end())
	{
		attribute = resource->add_attributes();
		attribute->set_key(OTLP_METHOD_NAME);
		value = attribute->mutable_value();
		value->set_string_value(iter->second);
	}

	for (const auto& attr : attributes)
	{
		KeyValue *attribute = resource->add_attributes();
		attribute->set_key(attr.first);
		AnyValue *value = attribute->mutable_value();
		value->set_string_value(attr.second);
	}

	// if attributes also set service.name, data takes precedence
	iter = data.find(OTLP_SERVICE_NAME);
	if (iter != data.end())
	{
		attribute = resource->add_attributes();
		attribute->set_key(OTLP_SERVICE_NAME);
		value = attribute->mutable_value();
		value->set_string_value(iter->second);
	}

	return spans;
}

static void rpc_span_fill_pb_span(RPCModuleData& data,
				const std::unordered_map<std::string, std::string>& attributes,
				InstrumentationLibrarySpans *spans)
{
	Span *span = spans->add_spans();
	Status *status = span->mutable_status();
	KeyValue *attribute;
	AnyValue *attr_value;

	for (const auto& attr : attributes)
	{
		attribute = span->add_attributes();
		attribute->set_key(attr.first);
		attr_value = attribute->mutable_value();
		attr_value->set_string_value(attr.second);
	}

	span->set_span_id(data[SRPC_SPAN_ID].c_str(), SRPC_SPANID_SIZE);
	span->set_trace_id(data[SRPC_TRACE_ID].c_str(), SRPC_TRACEID_SIZE);

	// name is required and specified in OpenTelemetry semantic conventions.
	if (data.find(OTLP_METHOD_NAME) != data.end())
	{
		span->set_name(data[OTLP_METHOD_NAME]); // for RPC
		attribute= span->add_attributes();
		attribute->set_key(SRPC_COMPONENT_OTEL_STR); // srpc.component -> rpc.system
		attr_value = attribute->mutable_value();
		attr_value->set_string_value(data[SRPC_COMPONENT]);
	}
	else
		span->set_name(data[SRPC_HTTP_METHOD]); // for HTTP

	// refer to : trace/semantic_conventions/http/#status
	int http_status_code = 0;
	auto iter = data.find(SRPC_HTTP_STATUS_CODE);
	if (iter != data.end())
		http_status_code = atoi(iter->second.c_str());

	for (const auto& kv : data)
	{
		const std::string& key = kv.first;
		const std::string& val = kv.second;

		if (key.compare(SRPC_PARENT_SPAN_ID) == 0)
		{
			span->set_parent_span_id(val);
		}
		else if (key.compare(SRPC_SPAN_KIND) == 0)
		{
			if (val.compare(SRPC_SPAN_KIND_CLIENT) == 0)
			{
				span->set_kind(Span_SpanKind_SPAN_KIND_CLIENT);
				if (http_status_code >= 400)
					status->set_code(Status_StatusCode_STATUS_CODE_ERROR);
			}
			else if (val.compare(SRPC_SPAN_KIND_SERVER) == 0)
			{
				span->set_kind(Span_SpanKind_SPAN_KIND_SERVER);
				if (http_status_code >= 500)
					status->set_code(Status_StatusCode_STATUS_CODE_ERROR);
			}
		}
		else if (key.compare(SRPC_START_TIMESTAMP) == 0)
		{
			span->set_start_time_unix_nano(atoll(data[SRPC_START_TIMESTAMP].data()));
		}
		else if (key.compare(SRPC_FINISH_TIMESTAMP) == 0)
		{
			span->set_end_time_unix_nano(atoll(data[SRPC_FINISH_TIMESTAMP].data()));
		}
		else if (key.compare(SRPC_STATE) == 0)
		{
			int state = atoi(val.c_str());
			if (state == RPCStatusOK)
				status->set_code(Status_StatusCode_STATUS_CODE_OK);
			else
				status->set_code(Status_StatusCode_STATUS_CODE_ERROR);
		}
		else if (key.compare(WF_TASK_STATE) == 0)
		{
			int state = atoi(val.c_str());
			if (state == WFT_STATE_SUCCESS)
				status->set_code(Status_StatusCode_STATUS_CODE_OK);
			else
				status->set_code(Status_StatusCode_STATUS_CODE_ERROR);
		}
		else if (key.compare(0, 5, "srpc.") != 0)
		{
			attribute= span->add_attributes();
			attribute->set_key(key);
			attr_value = attribute->mutable_value();

			size_t len = key.length();
			if ((len > 4 && key.substr(len - 4).compare("port") == 0) ||
				(len > 5 && key.substr(len - 5).compare("count") == 0) ||
				(len > 6 && key.substr(len - 6).compare("length") == 0) ||
				key.compare(SRPC_HTTP_STATUS_CODE)== 0)
			{
				attr_value->set_int_value(atoi(val.c_str()));
			}
			else
			{
				attr_value->set_string_value(val);
			}
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

	size_t ret = snprintf(str, len, "trace_id: %s span_id: %s",
						  trace_id_buf, span_id_buf);

	for (const auto& iter : data)
	{
		if (strcmp(iter.first.c_str(), SRPC_TRACE_ID) == 0 ||
			strcmp(iter.first.c_str(), SRPC_SPAN_ID) == 0 ||
			strcmp(iter.first.c_str(), SRPC_FINISH_TIMESTAMP) == 0 ||
			strcmp(iter.first.c_str(), SRPC_DURATION) == 0)
		{
			continue;
		}

		if (strcmp(iter.first.c_str(), SRPC_PARENT_SPAN_ID) == 0)
		{
			char parent_span_id_buf[SRPC_SPANID_SIZE * 2 + 1];
			span_id = (const uint64_t *)iter.second.c_str();

			SPAN_ID_BIN_TO_HEX(span_id, parent_span_id_buf);
			ret += snprintf(str + ret, len - ret, " parent_span_id: %s",
							parent_span_id_buf);
		}
		else if (strcmp(iter.first.c_str(), SRPC_START_TIMESTAMP) == 0)
		{
			ret += snprintf(str + ret, len - ret,
							" start_time: %s finish_time: %s duration: %s(ns)",
							data[SRPC_START_TIMESTAMP].c_str(),
							data[SRPC_FINISH_TIMESTAMP].c_str(),
							data[SRPC_DURATION].c_str());
		}
		else if (strcmp(iter.first.c_str(), SRPC_STATE) == 0 ||
				 strcmp(iter.first.c_str(), SRPC_ERROR) == 0)
		{
			ret += snprintf(str + ret, len - ret, " %s: %s",
							iter.first.c_str(), iter.second.c_str());
		}
/*
		else if (strcmp(it.first.c_str(), SRPC_SPAN_LOG) == 0)
		{
			ret += snprintf(str + ret, len - ret,
							"\n%s trace_id: %s span_id: %s"
							" timestamp: %s %s",
							"[ANNOTATION]",
							trace_id_buf,
							span_id_buf,
							it.first.c_str() + strlen(SRPC_SPAN_LOG) + 1,
							it.second.c_str());
		}
*/
		else
		{
			const char *key = iter.first.c_str();
			if (iter.first.compare(0, 5, "srpc.") == 0)
				key += 5;
			ret += snprintf(str + ret, len - ret, " %s: %s",
							key, iter.second.c_str());
		}
	}

	return ret;
}

bool RPCTraceFilterPolicy::collect(RPCModuleData& span)
{
	if (span.find(SRPC_TRACE_ID) == span.end())
		return false;

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

bool RPCTraceFilterPolicy::report(size_t count)
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

void RPCTraceLogTask::dispatch()
{
	char str[SPAN_LOG_MAX_LENGTH];
	rpc_span_log_format(this->span, str, SPAN_LOG_MAX_LENGTH);
	fprintf(stderr, "[SPAN_LOG] %s\n", str);

	this->subtask_done();
}

SubTask *RPCTraceRedis::create(RPCModuleData& span)
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

RPCTraceOpenTelemetry::RPCTraceOpenTelemetry(const std::string& url) :
	RPCFilter(RPCModuleTypeTrace),
	url(url + OTLP_TRACES_PATH),
	redirect_max(OTLP_HTTP_REDIRECT_MAX),
	retry_max(OTLP_HTTP_RETRY_MAX),
	filter_policy(SPANS_PER_SECOND_DEFAULT,
				  RPC_REPORT_THREHOLD_DEFAULT,
				  RPC_REPORT_INTERVAL_DEFAULT),
	report_status(false),
	report_span_count(0)
{
	this->report_req = new TracesData;
}

RPCTraceOpenTelemetry::RPCTraceOpenTelemetry(const std::string& url,
											 int redirect_max,
											 int retry_max,
											 size_t spans_per_second,
											 size_t report_threshold,
											 size_t report_interval) :
	RPCFilter(RPCModuleTypeTrace),
	url(url + OTLP_TRACES_PATH),
	redirect_max(redirect_max),
	retry_max(retry_max),
	filter_policy(spans_per_second, report_threshold, report_interval),
	report_status(false),
	report_span_count(0)
{
	this->report_req = new TracesData;
}

RPCTraceOpenTelemetry::~RPCTraceOpenTelemetry()
{
	delete this->report_req;
}

SubTask *RPCTraceOpenTelemetry::create(RPCModuleData& span)
{
	std::string *output = new std::string;
	SubTask *next = NULL;
	TracesData *req = (TracesData *)this->report_req;

	this->mutex.lock();
	if (!this->report_status)
		next = WFTaskFactory::create_empty_task();
	else
	{
//		fprintf(stderr, "[Trace info to report]\n%s\n\n", req->DebugString().c_str());
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
/*
		protocol::HttpResponse *resp = task->get_resp();
		fprintf(stderr, "[Trace report callback] state=%d error=%d\n",
				task->get_state(), task->get_error());

		if (task->get_state() == WFT_STATE_SUCCESS)
		{
			fprintf(stderr, "%s %s %s\r\n", resp->get_http_version(),
					resp->get_status_code(), resp->get_reason_phrase());
		}
*/
		delete (std::string *)task->user_data;
	});

	protocol::HttpRequest *http_req = task->get_req();
	http_req->set_method(HttpMethodPost);
	http_req->add_header_pair("Content-Type", "application/x-protobuf");

	task->user_data = output;
	http_req->append_output_body_nocopy(output->c_str(), output->length());

	return task;
}

void RPCTraceOpenTelemetry::add_attributes(const std::string& key,
										   const std::string& value)
{
	this->mutex.lock();
	this->attributes.insert(std::make_pair(key, value));
	this->mutex.unlock();
}

void RPCTraceOpenTelemetry::add_span_attributes(const std::string& key,
												const std::string& value)
{
	this->mutex.lock();
	this->span_attributes.insert(std::make_pair(key, value));
	this->mutex.unlock();
}

size_t RPCTraceOpenTelemetry::clear_attributes()
{
	size_t ret;

	this->mutex.lock();
	ret = this->attributes.size();
	this->attributes.clear();
	this->mutex.unlock();

	return ret;
}

size_t RPCTraceOpenTelemetry::clear_span_attributes()
{
	size_t ret;

	this->mutex.lock();
	ret = this->span_attributes.size();
	this->span_attributes.clear();
	this->mutex.unlock();

	return ret;
}

bool RPCTraceOpenTelemetry::filter(RPCModuleData& data)
{
	std::unordered_map<std::string, google::protobuf::Message *>::iterator it;
	InstrumentationLibrarySpans *spans;
	std::string service_name;
	bool ret;

	auto iter = data.find(OTLP_SERVICE_NAME);
	if (iter != data.end())
	{
		service_name = iter->second;
	}
	else // for HTTP
	{
		service_name = data[SRPC_COMPONENT] + std::string(".") +
					   data[SRPC_HTTP_SCHEME];

		if (data.find(SRPC_SPAN_KIND_CLIENT) != data.end())
			service_name += ".client";
		else
			service_name += ".server";
	}

	this->mutex.lock();
	if (this->filter_policy.collect(data))
	{
		++this->report_span_count;
		it = this->report_map.find(service_name);
		if (it == this->report_map.end())
		{
			spans = rpc_span_fill_pb_request(data, this->attributes,
											 (TracesData *)this->report_req);
			this->report_map.insert({service_name, spans});
		}
		else
			spans = (InstrumentationLibrarySpans *)it->second;

		rpc_span_fill_pb_span(data, this->span_attributes, spans);
	}

	ret = this->filter_policy.report(this->report_span_count);
	if (ret)
		this->report_status = true;
	this->mutex.unlock();

	return ret;
}

} // end namespace srpc

