#include <stdio.h>
#include <limits>
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHttpServer.h"
#include "rpc_filter_metrics.h"
#include "opentelemetry_metrics_service.pb.h"

namespace srpc
{

using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

RPCMetricsPull::RPCMetricsPull() :
	collector(this->report_output),
	server(std::bind(&RPCMetricsPull::pull,
					 this, std::placeholders::_1))
{
}

bool RPCMetricsPull::init(short port)
{
	if (this->server.start(port) == 0)
		return true;

	return false;
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
	std::unordered_map<std::string, RPCVar*> tmp;
	std::unordered_map<std::string, RPCVar*>::iterator it;
	std::string output;

	RPCVarGlobal *global_var = RPCVarGlobal::get_instance();

	global_var->mutex.lock();
	for (RPCVarLocal *local : global_var->local_vars)
	{
		local->mutex.lock();
		for (it = local->vars.begin(); it != local->vars.end(); it++)
		{
			if (this->var_names.find(it->first) == this->var_names.end())
				continue;

			if (tmp.find(it->first) == tmp.end())
				tmp.insert(std::make_pair(it->first, it->second->create(true)));
			else
				tmp[it->first]->reduce(it->second->get_data(),
									   it->second->get_size());
		}
		local->mutex.unlock();
	}
	global_var->mutex.unlock();

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

void RPCMetricsPull::Collector::collect_gauge(RPCVar *gauge, int64_t data)
{
	this->report_output += gauge->get_name() + " " + std::to_string(data) + "\n";
}

void RPCMetricsPull::Collector::collect_gauge(RPCVar *gauge, double data)
{
	this->report_output += gauge->get_name() + " " + std::to_string(data) + "\n";
}

void RPCMetricsPull::Collector::collect_counter_each(RPCVar *counter,
													 const std::string& label,
													 int64_t data)
{
	this->report_output += counter->get_name() + "{" + label + "} " +
						   std::to_string(data) + "\n";
}

void RPCMetricsPull::Collector::collect_counter_each(RPCVar *counter,
													 const std::string& label,
													 double data)
{
	this->report_output += counter->get_name() + "{" + label + "} " +
						   std::to_string(data) + "\n";
}

void RPCMetricsPull::Collector::collect_histogram_each(RPCVar *histogram,
													   int64_t bucket_boundary,
													   size_t current_count)
{
	this->report_output += histogram->get_name();
	if (bucket_boundary != std::numeric_limits<int64_t>::max())
	{
		this->report_output += histogram->get_name() + "_bucket{le=\"" +
							   std::to_string(bucket_boundary) + "\"} ";
	}
	else
		this->report_output += "_bucket{le=\"+Inf\"} ";

	this->report_output += std::to_string(current_count) + "\n";
}

void RPCMetricsPull::Collector::collect_histogram_sum(RPCVar *histogram,
													  int64_t sum,
													  size_t count)
{
	this->report_output += histogram->get_name() + "_sum " +
						   std::to_string(sum) + "\n";
	this->report_output += histogram->get_name() + "_count " +
						   std::to_string(count) + "\n";
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
													 int64_t quantile_out,
													 size_t available_count)
{
	this->report_output += summary->get_name() + "{quantile=\"" +
						   std::to_string(quantile) + "\"} ";
	if (quantile_out == std::numeric_limits<int64_t>::quiet_NaN())
		this->report_output += "NaN";
	else
		this->report_output += std::to_string(quantile_out / available_count);
	this->report_output += "\n";
}

void RPCMetricsPull::Collector::collect_summary_sum(RPCVar *summary,
														   int64_t sum,
														   size_t count)
{
	this->report_output += summary->get_name() + "_sum " +
						   std::to_string(sum) + "\n";
	this->report_output += summary->get_name() + "_count " +
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

RPCMetricsOTel::RPCMetricsOTel(/* policy params */)
{
	this->report_req = new ExportMetricsServiceRequest();
	this->collector.set_report_req(this->report_req);
}

bool RPCMetricsOTel::expose()
{
	return true;
}

void RPCMetricsOTel::Collector::collect_gauge(RPCVar *gauge, int64_t data)
{
}

void RPCMetricsOTel::Collector::collect_gauge(RPCVar *gauge, double data)
{
}

void RPCMetricsOTel::Collector::collect_counter_each(RPCVar *counter,
													 const std::string& label,
													 int64_t data)
{
}

void RPCMetricsOTel::Collector::collect_counter_each(RPCVar *counter,
													 const std::string& label,
													 double data)
{
}

void RPCMetricsOTel::Collector::collect_histogram_each(RPCVar *histogram,
													   int64_t bucket_boundary,
													   size_t current_count)
{
}

void RPCMetricsOTel::Collector::collect_histogram_sum(RPCVar *histogram,
													  int64_t sum,
													  size_t count)
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
													 int64_t quantile_out,
													 size_t available_count)
{
}

void RPCMetricsOTel::Collector::collect_summary_sum(RPCVar *summary,
													int64_t sum,
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

