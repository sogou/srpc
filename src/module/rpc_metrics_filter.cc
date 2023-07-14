#include <stdio.h>
#include <cfloat>
#include <string>
#include <set>
#include <vector>
#include <mutex>
#include <chrono>
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHttpServer.h"
#include "rpc_basic.h"
#include "rpc_var.h"
#include "rpc_metrics_filter.h"
#include "opentelemetry_metrics_service.pb.h"

namespace srpc
{

static constexpr size_t		 METRICS_DEFAULT_MAX_AGE	= 60;
static constexpr size_t		 METRICS_DEFAULT_AGE_BUCKET	= 5;
static constexpr const char	*METRICS_PULL_METRICS_PATH	= "/metrics";
static constexpr const char	*OTLP_METRICS_PATH			= "/v1/metrics";
static constexpr const char *METRICS_REQUEST_COUNT		= "total_request_count";
static constexpr const char *METRICS_REQUEST_METHOD		= "total_request_method";
static constexpr const char *METRICS_REQUEST_LATENCY	= "total_request_latency";
//static constexpr const char *METRICS_REQUEST_SIZE		= "total_request_size";
//static constexpr const char *METRICS_RESPONSE_SIZE	= "total_response_size";

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

RPCMetricsFilter::RPCMetricsFilter() :
	RPCFilter(RPCModuleTypeMetrics)
{
	this->create_gauge(METRICS_REQUEST_COUNT, "total request count");
	this->create_counter(METRICS_REQUEST_METHOD, "request method statistics");
//	this->create_histogram(METRICS_REQUEST_SIZE, "total request body size",
//						   {256, 512, 1024, 16384});
//	this->create_histogram(METRICS_RESPONSE_SIZE, "total response body size",
//						   {256, 512, 1024, 16384});
	this->create_summary(METRICS_REQUEST_LATENCY, "request latency nano seconds",
						 {{0.5, 0.05}, {0.9, 0.01}});
}

bool RPCMetricsFilter::client_end(SubTask *task, RPCModuleData& data)
{
	this->gauge(METRICS_REQUEST_COUNT)->increase();
	this->counter(METRICS_REQUEST_METHOD)->add(
				{{"service", data[OTLP_SERVICE_NAME]},
				 {"method",  data[OTLP_METHOD_NAME] }})->increase();
	this->summary(METRICS_REQUEST_LATENCY)->observe(atoll(data[SRPC_DURATION].data()));

	return true;
}

bool RPCMetricsFilter::server_end(SubTask *task, RPCModuleData& data)
{
	this->gauge(METRICS_REQUEST_COUNT)->increase();
	this->counter(METRICS_REQUEST_METHOD)->add(
				{{"service", data[OTLP_SERVICE_NAME]},
				 {"method",  data[OTLP_METHOD_NAME] }})->increase();
	this->summary(METRICS_REQUEST_LATENCY)->observe(atoll(data[SRPC_DURATION].data()));

	return true;
}

GaugeVar *RPCMetricsFilter::gauge(const std::string& name)
{
	return RPCVarFactory::gauge(name);
}

CounterVar *RPCMetricsFilter::counter(const std::string& name)
{
	return RPCVarFactory::counter(name);
}

HistogramVar *RPCMetricsFilter::histogram(const std::string& name)
{
	return RPCVarFactory::histogram(name);
}

SummaryVar *RPCMetricsFilter::summary(const std::string& name)
{
	return RPCVarFactory::summary(name);
}

GaugeVar *RPCMetricsFilter::create_gauge(const std::string& name,
										 const std::string& help)
{
	if (RPCVarFactory::check_name_format(name) == false)
	{
		errno = EINVAL;
		return NULL;
	}

	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	GaugeVar *gauge = new GaugeVar(name, help);
	RPCVarLocal::get_instance()->add(name, gauge);
	return gauge;
}

CounterVar *RPCMetricsFilter::create_counter(const std::string& name,
											 const std::string& help)
{
	if (RPCVarFactory::check_name_format(name) == false)
	{
		errno = EINVAL;
		return NULL;
	}

	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	CounterVar *counter = new CounterVar(name, help);
	RPCVarLocal::get_instance()->add(name, counter);
	return counter;
}

HistogramVar *RPCMetricsFilter::create_histogram(const std::string& name,
												 const std::string& help,
												 const std::vector<double>& bucket)
{
	if (RPCVarFactory::check_name_format(name) == false)
	{
		errno = EINVAL;
		return NULL;
	}

	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	HistogramVar *histogram = new HistogramVar(name, help, bucket);
	RPCVarLocal::get_instance()->add(name, histogram);
	return histogram;
}

SummaryVar *RPCMetricsFilter::create_summary(const std::string& name,
											 const std::string& help,
								const std::vector<struct Quantile>& quantile)
{
	if (RPCVarFactory::check_name_format(name) == false)
	{
		errno = EINVAL;
		return NULL;
	}

	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	SummaryVar *summary = new SummaryVar(name, help, quantile,
								std::chrono::seconds(METRICS_DEFAULT_MAX_AGE),
								METRICS_DEFAULT_AGE_BUCKET);

	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

SummaryVar *RPCMetricsFilter::create_summary(const std::string& name,
											 const std::string& help,
								const std::vector<struct Quantile>& quantile,
								const std::chrono::milliseconds max_age,
								int age_bucket)
{
	if (RPCVarFactory::check_name_format(name) == false)
	{
		errno = EINVAL;
		return NULL;
	}

	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	SummaryVar *summary = new SummaryVar(name, help, quantile, max_age, age_bucket);
	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

void RPCMetricsFilter::reduce(std::unordered_map<std::string, RPCVar *>& out)
{
	std::unordered_map<std::string, RPCVar *>::iterator it;
	RPCVarGlobal *global_var = RPCVarGlobal::get_instance();

	global_var->mutex.lock();
	for (RPCVarLocal *local : global_var->local_vars)
	{
		local->mutex.lock();
		for (it = local->vars.begin(); it != local->vars.end(); it++)
		{
			if (this->var_names.find(it->first) == this->var_names.end())
				continue;

			if (out.find(it->first) == out.end())
				out.insert(std::make_pair(it->first, it->second->create(true)));
			else
				out[it->first]->reduce(it->second->get_data(),
									   it->second->get_size());
		}
		local->mutex.unlock();
	}
	global_var->mutex.unlock();
}

RPCMetricsPull::RPCMetricsPull() :
	collector(this->report_output),
	server(std::bind(&RPCMetricsPull::pull, this, std::placeholders::_1))
{
}

bool RPCMetricsPull::init(unsigned short port)
{
	return this->server.start(port) == 0;
}

void RPCMetricsPull::deinit()
{
	this->server.stop();
}

void RPCMetricsPull::pull(WFHttpTask *task)
{
	if (strcmp(task->get_req()->get_request_uri(), METRICS_PULL_METRICS_PATH))
		return;

	this->mutex.lock();
	this->expose();
	task->get_resp()->append_output_body(std::move(this->report_output));
	this->report_output.clear();
	this->mutex.unlock();
}

bool RPCMetricsPull::expose()
{
	std::unordered_map<std::string, RPCVar *> tmp;
	std::unordered_map<std::string, RPCVar *>::iterator it;
	std::string output;

	this->reduce(tmp);

	for (it = tmp.begin(); it != tmp.end(); it++)
	{
		RPCVar *var = it->second;
		this->report_output += "# HELP " + var->get_name() + " " +
							   var->get_help() + "\n# TYPE " + var->get_name() +
                               " " + var->get_type_str() + "\n";
		output.clear();
		var->collect(&this->collector);
		this->report_output += output;
	}

	for (it = tmp.begin(); it != tmp.end(); it++)
		delete it->second;

	return true;
}

void RPCMetricsPull::Collector::collect_gauge(RPCVar *gauge, double data)
{
	this->report_output += gauge->get_name() + " " + std::to_string(data) + "\n";
}

void RPCMetricsPull::Collector::collect_counter_each(RPCVar *counter,
													 const std::string& label,
													 double data)
{
	this->report_output += counter->get_name() + "{" + label + "} " +
						   std::to_string(data) + "\n";
}

void RPCMetricsPull::Collector::collect_histogram_each(RPCVar *histogram,
													   double bucket_boundary,
													   size_t current_count)
{
	this->report_output += histogram->get_name();
	if (bucket_boundary != DBL_MAX)
	{
		this->report_output += "_bucket{le=\"" +
							   std::to_string(bucket_boundary) + "\"} ";
	}
	else
		this->report_output += "_bucket{le=\"+Inf\"} ";

	this->report_output += std::to_string(current_count) + "\n";
}

void RPCMetricsPull::Collector::collect_histogram_end(RPCVar *histogram,
													  double sum,
													  size_t count)
{
	this->report_output += histogram->get_name() + "_sum " +
						   std::to_string(sum) + "\n";
	this->report_output += histogram->get_name() + "_count " +
						   std::to_string(count) + "\n";
}

void RPCMetricsPull::Collector::collect_summary_each(RPCVar *summary,
													 double quantile,
													 double quantile_out)
{
	this->report_output += summary->get_name() + "{quantile=\"" +
						   std::to_string(quantile) + "\"} ";
	if (quantile_out == 0)
		this->report_output += "NaN";
	else
		this->report_output += std::to_string(quantile_out);
	this->report_output += "\n";
}

void RPCMetricsPull::Collector::collect_summary_end(RPCVar *summary,
													double sum,
													size_t count)
{
	this->report_output += summary->get_name() + "_sum " +
						   std::to_string(sum) + "\n";
	this->report_output += summary->get_name() + "_count " +
						   std::to_string(count) + "\n";
}

bool RPCFilterPolicy::report(size_t count)
{
	long long timestamp = GET_CURRENT_MS();

	if (this->last_report_timestamp == 0)
		this->last_report_timestamp = timestamp;

	if (timestamp > this->last_report_timestamp +
		(long long)this->report_interval ||
		count >= this->report_threshold)
	{
		this->last_report_timestamp = timestamp;
		return true;
	}

	return false;
}

RPCMetricsOTel::RPCMetricsOTel(const std::string& url,
							   unsigned int redirect_max,
							   unsigned int retry_max,
							   size_t report_threshold,
							   size_t report_interval_msec) :
	url(url + OTLP_METRICS_PATH),
	redirect_max(redirect_max),
	retry_max(retry_max),
	policy(report_threshold, report_interval_msec),
	report_counts(0)
{
}

RPCMetricsOTel::RPCMetricsOTel(const std::string& url) :
	url(url + OTLP_METRICS_PATH),
	redirect_max(OTLP_HTTP_REDIRECT_MAX),
	retry_max(OTLP_HTTP_RETRY_MAX),
	policy(RPC_REPORT_THREHOLD_DEFAULT, RPC_REPORT_INTERVAL_DEFAULT),
	report_counts(0)
{
}

RPCMetricsOTel::Collector::~Collector()
{
	for (const auto& kv : this->label_map)
		delete kv.second;
}

void RPCMetricsOTel::add_attributes(const std::string& key,
									const std::string& value)
{
	this->mutex.lock();
	this->attributes.insert(std::make_pair(key, value));
	this->mutex.unlock();
}

size_t RPCMetricsOTel::clear_attributes()
{
	size_t ret;

	this->mutex.lock();
	ret = this->attributes.size();
	this->attributes.clear();
	this->mutex.unlock();

	return ret;
}

SubTask *RPCMetricsOTel::create(RPCModuleData& data)
{
	std::string *output = new std::string;
	ExportMetricsServiceRequest req;
	ResourceMetrics *rm = req.add_resource_metrics();
	Resource *resource = rm->mutable_resource();
	ScopeMetrics *metrics = rm->add_scope_metrics();
	KeyValue *attribute;
	AnyValue *value;

	InstrumentationScope *scope = metrics->mutable_scope();
	scope->set_name(this->scope_name);

	auto iter = data.find(OTLP_METHOD_NAME);

	if (iter != data.end())
	{
		attribute = resource->add_attributes();
		attribute->set_key(OTLP_METHOD_NAME);
		value = attribute->mutable_value();
		value->set_string_value(iter->second);
	}

	this->mutex.lock();
	for (const auto& attr : this->attributes)
	{
		KeyValue *attribute = resource->add_attributes();
		attribute->set_key(attr.first);
		AnyValue *value = attribute->mutable_value();
		value->set_string_value(attr.second);
	}	
	this->mutex.unlock();

	iter = data.find(OTLP_SERVICE_NAME); // if attributes also set service.name, data takes precedence
	if (iter != data.end())
	{
		attribute = resource->add_attributes();
		attribute->set_key(OTLP_SERVICE_NAME);
		value = attribute->mutable_value();
		value->set_string_value(iter->second);
	}

	this->expose(metrics);

//	fprintf(stderr, "[Metrics info to report]\n%s\n", req.DebugString().c_str());
	req.SerializeToString(output);
	this->report_counts = 0;

	WFHttpTask *task = WFTaskFactory::create_http_task(this->url,
													   this->redirect_max,
													   this->retry_max,
													   [](WFHttpTask *task) {
/*
		protocol::HttpResponse *resp = task->get_resp();
		fprintf(stderr, "[metrics report callback] state=%d error=%d\n",
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

bool RPCMetricsOTel::expose(google::protobuf::Message *msg)
{
	std::unordered_map<std::string, RPCVar *> tmp;
	std::unordered_map<std::string, RPCVar *>::iterator it;
	ScopeMetrics *metrics;
	metrics = static_cast<ScopeMetrics *>(msg);

	this->reduce(tmp);

	for (it = tmp.begin(); it != tmp.end(); it++)
	{
		RPCVar *var = it->second;
		Metric *m = metrics->add_metrics();
		google::protobuf::Message *current_var;

		m->set_name(var->get_name());
		m->set_description(var->get_help());

		switch(var->get_type())
		{
		default:
		case VAR_GAUGE:
			current_var = m->mutable_gauge();
			break;
		case VAR_COUNTER:
			current_var = m->mutable_sum();
			break;
		case VAR_HISTOGRAM:
			current_var = m->mutable_histogram();
			break;
		case VAR_SUMMARY:
			current_var = m->mutable_summary();
			break;
		}

		this->collector.set_current_message(current_var);
		this->collector.set_current_nano(GET_CURRENT_NS());
		var->collect(&this->collector);
	}

	for (it = tmp.begin(); it != tmp.end(); it++)
		delete it->second;

	return true;
}

void RPCMetricsOTel::Collector::collect_gauge(RPCVar *gauge, double data)
{
	Gauge *report_gauge = static_cast<Gauge *>(this->current_msg);
	NumberDataPoint *data_points = report_gauge->add_data_points();
	data_points->set_as_double(data);
	data_points->set_time_unix_nano(this->current_timestamp_nano);
}

void RPCMetricsOTel::Collector::add_counter_label(const std::string& label)
{
	const char *key;
	const char *value;
	size_t key_len;
	size_t lpos;
	size_t rpos;
	size_t pos = -1;

	LABEL_MAP *m = new LABEL_MAP();

	do {
		pos++;
		lpos = label.find_first_of('=', pos);
		key = label.data() + pos;
		key_len = lpos - pos;

		rpos = label.find_first_of(',', lpos + 1);
		pos = rpos;
		if (rpos == std::string::npos)
			rpos = label.length();
		value = label.data() + lpos + 2;
//		value_len = rpos - lpos - 3;

		m->emplace(std::string{key, key_len}, std::string{value, rpos - lpos - 3});
	} while (pos != std::string::npos);

	this->label_map.emplace(label, m);
}

void RPCMetricsOTel::Collector::collect_counter_each(RPCVar *counter,
													 const std::string& label,
													 double data)
{
	Sum *report_sum = static_cast<Sum *>(this->current_msg);
	NumberDataPoint *data_points = report_sum->add_data_points();
	std::map<std::string, LABEL_MAP *>::iterator it = this->label_map.find(label);
	std::string key;
	std::string value;

	if (it == this->label_map.end())
	{
		this->add_counter_label(label);
		it = this->label_map.find(label);
	}

	for (const auto& kv : *(it->second))
	{
		KeyValue *attribute = data_points->add_attributes();
		attribute->set_key(kv.first);
		AnyValue *value = attribute->mutable_value();
		value->set_string_value(kv.second);
	}

	data_points->set_as_double(data);
	data_points->set_time_unix_nano(this->current_timestamp_nano);
}

void RPCMetricsOTel::Collector::collect_histogram_begin(RPCVar *histogram)
{
	Histogram *report_histogram = static_cast<Histogram *>(this->current_msg);
	HistogramDataPoint *data_points = report_histogram->add_data_points();
	this->current_msg = data_points;
	data_points->set_time_unix_nano(this->current_timestamp_nano);
}

void RPCMetricsOTel::Collector::collect_histogram_each(RPCVar *histogram,
													   double bucket_boundary,
													   size_t current_count)
{
	HistogramDataPoint *data_points = static_cast<HistogramDataPoint *>(this->current_msg);
	data_points->add_bucket_counts(current_count);

	if (bucket_boundary != DBL_MAX)
		data_points->add_explicit_bounds(bucket_boundary);
}

void RPCMetricsOTel::Collector::collect_histogram_end(RPCVar *histogram,
													  double sum,
													  size_t count)
{
	HistogramDataPoint *data_points = static_cast<HistogramDataPoint *>(this->current_msg);
	data_points->set_sum(sum);
	data_points->set_count(count);
}

void RPCMetricsOTel::Collector::collect_summary_begin(RPCVar *summary)
{
	Summary *report_summary = static_cast<Summary *>(this->current_msg);
	SummaryDataPoint *data_points = report_summary->add_data_points();
	this->current_msg = data_points;
	data_points->set_time_unix_nano(this->current_timestamp_nano);
}

void RPCMetricsOTel::Collector::collect_summary_each(RPCVar *summary,
													 double quantile,
													 double quantile_out)
{
	SummaryDataPoint *data_points = static_cast<SummaryDataPoint *>(this->current_msg);
	SummaryDataPoint::ValueAtQuantile *vaq = data_points->add_quantile_values();
	vaq->set_quantile(quantile);
	vaq->set_value(quantile_out);
}

void RPCMetricsOTel::Collector::collect_summary_end(RPCVar *summary,
													double sum,
													size_t count)
{
	SummaryDataPoint *data_points = static_cast<SummaryDataPoint *>(this->current_msg);
	data_points->set_sum(sum);
	data_points->set_count(count);
}

} // end namespace srpc

