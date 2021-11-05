#include <stdio.h>
#include <limits.h>
#include "workflow/WFTask.h"
#include "rpc_span_policies.h"
#include "trace_service.pb.h"

#include <google/protobuf/util/json_util.h>                                     
#include <google/protobuf/util/type_resolver_util.h>

namespace srpc
{

static constexpr const char* kTypeUrlPrefix = "type.googleapis.com";

class ResolverInstance
{
public:
	static google::protobuf::util::TypeResolver* get_resolver()
	{
		static ResolverInstance kInstance;
		return kInstance.resolver_;
	}

private:
	ResolverInstance()
	{
		resolver_ = google::protobuf::util::NewTypeResolverForDescriptorPool(kTypeUrlPrefix,
									google::protobuf::DescriptorPool::generated_pool());
	}

	~ResolverInstance() { delete resolver_; }

	google::protobuf::util::TypeResolver* resolver_;
};

static inline std::string GetTypeUrl(const ProtobufIDLMessage *pb_msg)
{
	return std::string(kTypeUrlPrefix) + "/" + pb_msg->GetDescriptor()->full_name();
}

static size_t rpc_span_pb_format(RPCModuleData& data,
opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest& req)
{
	///TODO:
	auto resource_span = req.add_resource_spans();
	auto instrumentation_lib = resource_span->add_instrumentation_library_spans();
	auto span = instrumentation_lib->add_spans();

	span->set_span_id("1412");//data[SRPC_SPAN_ID]);
	span->set_trace_id(data[SRPC_TRACE_ID]);
	span->set_name(data[SRPC_METHOD_NAME]);
	auto iter = data.find(SRPC_PARENT_SPAN_ID);
	if (iter != data.end())
		span->set_parent_span_id(iter->second);
	
//	resource_span->mutable_resource() = rec->ProtoResource();
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

	for (auto &it : data)
	{
		if (strncmp(it.first.c_str(), SRPC_SPAN_LOG, 3) == 0)
			ret += snprintf(str + ret, len - ret, "\n%s trace_id: %s span_id: %s"
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

	opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest req;
	rpc_span_pb_format(span, req);
//	void *buffer = malloc(req.ByteSize());

	std::string binary_output;//TODO: remember to save it till task deconstructed
/*
	std::string binary_input = req.SerializeAsString();                     

	const auto* pool = req.GetDescriptor()->file()->pool();                 
	auto* resolver = (pool == google::protobuf::DescriptorPool::generated_pool()
							? ResolverInstance::get_resolver()                      
							: google::protobuf::util::NewTypeResolverForDescriptorPool(kTypeUrlPrefix, pool));

	bool ret = BinaryToJsonString(resolver, GetTypeUrl(&req), binary_input, &binary_output).ok();
	if (pool != google::protobuf::DescriptorPool::generated_pool())             
		delete resolver;

*/
	fprintf(stderr, "get json string:\n%s\n", binary_output.c_str());

//	auto *task = WFTaskFactory::create_http_task("http://127.0.0.1:16686/v1/traces",
	auto *task = WFTaskFactory::create_http_task("http://127.0.0.1:80/v1/traces",
// + this->otel_url +
//												 "/" + SPAN_OPENTELEMETRY_PATH,
												 this->redirect_max,
												 this->retry_max,
												 [](WFHttpTask *t){
								fprintf(stderr, "span report callback: s=%d e=%d\n",
										t->get_state(), t->get_error());
												 });

	protocol::HttpRequest *http_req = task->get_req();
	http_req->append_output_body_nocopy(binary_output.c_str(), binary_output.length());
	return task;
}

}

