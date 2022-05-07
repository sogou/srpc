#include <stdio.h>
#include <limits>
#include <string>
#include <set>
#include <vector>
#include <mutex>
#include <chrono>
#include "rpc_basic.h"
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHttpServer.h"
#include "rpc_filter_metrics.h"
#include "rpc_var_factory.h"
#include "opentelemetry_metrics_service.pb.h"

namespace srpc
{

static constexpr size_t			METRICS_DEFAULT_MAX_AGE		= 60;
static constexpr size_t			METRICS_DEFAULT_AGE_BUCKET	= 5;
static constexpr const char	   *METRICS_PULL_METRICS_PATH	= "/metrics";
static constexpr const char	   *OTLP_METRICS_PATH	= "/v1/metrics";

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

MetricsGauge *RPCMetricsFilter::gauge(const std::string& name)
{
	return RPCVarFactory::gauge<double>(name);
}

MetricsCounter *RPCMetricsFilter::counter(const std::string& name)
{
	return RPCVarFactory::counter<double>(name);
}

MetricsHistogram *RPCMetricsFilter::histogram(const std::string& name)
{
	return RPCVarFactory::histogram<double>(name);
}

MetricsSummary *RPCMetricsFilter::summary(const std::string& name)
{
	return RPCVarFactory::summary<double>(name);
}

MetricsGauge *RPCMetricsFilter::create_gauge(const std::string& name,
											 const std::string& help)
{
	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	MetricsGauge *gauge = new MetricsGauge(name, help);
	RPCVarLocal::get_instance()->add(name, gauge);
	return gauge;
}

MetricsCounter *RPCMetricsFilter::create_counter(const std::string& name,
												 const std::string& help)
{
	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	MetricsCounter *counter = new MetricsCounter(name, help);
	RPCVarLocal::get_instance()->add(name, counter);
	return counter;
}

MetricsHistogram *RPCMetricsFilter::create_histogram(const std::string& name,
													 const std::string& help,
													 const std::vector<double>& bucket)
{
	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	MetricsHistogram *histogram = new MetricsHistogram(name, help, bucket);
	RPCVarLocal::get_instance()->add(name, histogram);
	return histogram;
}

MetricsSummary *RPCMetricsFilter::create_summary(const std::string& name,
												 const std::string& help,
								const std::vector<struct Quantile>& quantile)
{
	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	MetricsSummary *summary = new MetricsSummary(name, help, quantile,
								std::chrono::seconds(METRICS_DEFAULT_MAX_AGE),
								METRICS_DEFAULT_AGE_BUCKET);

	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

MetricsSummary *RPCMetricsFilter::create_summary(const std::string& name,
												 const std::string& help,
								const std::vector<struct Quantile>& quantile,
								const std::chrono::milliseconds max_age,
								int age_bucket)
{
	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	MetricsSummary *summary = new MetricsSummary(name, help, quantile, max_age,
												 age_bucket);
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
	if (bucket_boundary != std::numeric_limits<double>::max())
	{
		this->report_output += histogram->get_name() + "_bucket{le=\"" +
							   std::to_string(bucket_boundary) + "\"}";
	}
	else
		this->report_output += "_bucket{le=\"+Inf\"} ";

	this->report_output += std::to_string(current_count) + "\n";
}

void RPCMetricsPull::Collector::collect_histogram_sum(RPCVar *histogram,
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
													 double quantile_out,
													 size_t available_count)
{
	this->report_output += summary->get_name() + "{quantile=\"" +
						   std::to_string(quantile) + "\"} ";
	if (quantile_out == std::numeric_limits<double>::quiet_NaN())
		this->report_output += "NaN";
	else
		this->report_output += std::to_string(quantile_out / available_count);
	this->report_output += "\n";
}

void RPCMetricsPull::Collector::collect_summary_sum(RPCVar *summary,
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
	report_status(false),
	report_counts(0)
{
}

RPCMetricsOTel::RPCMetricsOTel(const std::string& url) :
	url(url + OTLP_METRICS_PATH),
	redirect_max(OTLP_HTTP_REDIRECT_MAX),
	retry_max(OTLP_HTTP_RETRY_MAX),
	policy(RPC_REPORT_THREHOLD_DEFAULT, RPC_REPORT_INTERVAL_DEFAULT),
	report_status(false),
	report_counts(0)
{
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
	InstrumentationLibraryMetrics *metrics = rm->add_instrumentation_library_metrics();

	this->mutex.lock();
	for (const auto& attr : attributes)
	{
		KeyValue *attribute = resource->add_attributes();
		attribute->set_key(attr.first);
		AnyValue *value = attribute->mutable_value();
		value->set_string_value(attr.second);
	}	

	KeyValue *attribute = resource->add_attributes();
	attribute->set_key(OTLP_SERVICE_NAME_KEY);
	AnyValue *value = attribute->mutable_value();
	value->set_string_value(data[OTLP_SERVICE_NAME_KEY]);

	this->expose(metrics);

	req.SerializeToString(output);
	this->report_counts = 0;

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

bool RPCMetricsOTel::expose(google::protobuf::Message *msg)
{
	std::unordered_map<std::string, RPCVar *> tmp;
	std::unordered_map<std::string, RPCVar *>::iterator it;
	InstrumentationLibraryMetrics *metrics;
	metrics = static_cast<InstrumentationLibraryMetrics *>(msg);

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
		default:
			return false;
		}

		this->collector.set_current_message(current_var);
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
}

void RPCMetricsOTel::Collector::collect_counter_each(RPCVar *counter,
													 const std::string& label,
													 double data)
{
}

void RPCMetricsOTel::Collector::collect_histogram_each(RPCVar *histogram,
													   double bucket_boundary,
													   size_t current_count)
{
}

void RPCMetricsOTel::Collector::collect_histogram_sum(RPCVar *histogram,
													  double sum,
													  size_t count)
{
}

void RPCMetricsOTel::Collector::collect_summary_each(RPCVar *summary,
													 double quantile,
													 double quantile_out,
													 size_t available_count)
{
}

void RPCMetricsOTel::Collector::collect_summary_sum(RPCVar *summary,
													double sum,
													size_t count)
{
}

} // end namespace srpc

