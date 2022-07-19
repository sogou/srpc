/*
  Copyright (c) 2022 Sogou, Inc.

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

#ifndef __RPC_FILTER_METRICS_H__
#define __RPC_FILTER_METRICS_H__

#include <string>
#include <set>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <atomic>
#include <google/protobuf/message.h>
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHttpServer.h"
#include "rpc_basic.h"
#include "rpc_var.h"
#include "rpc_module_metrics.h"

namespace srpc
{

class RPCMetricsFilter : public RPCFilter
{
public:
	GaugeVar *create_gauge(const std::string& name,
						   const std::string& help);

	CounterVar *create_counter(const std::string& name,
							   const std::string& help);

	HistogramVar *create_histogram(const std::string& name,
								   const std::string& help,
								   const std::vector<double>& bucket);

	SummaryVar *create_summary(const std::string& name,
							   const std::string& help,
							   const std::vector<struct Quantile>& quantile);

	SummaryVar *create_summary(const std::string& name,
							   const std::string& help,
							   const std::vector<struct Quantile>& quantile,
							   const std::chrono::milliseconds max_age,
							   int age_bucket);
	// thread local api
	GaugeVar *gauge(const std::string& name);
	CounterVar *counter(const std::string& name);
	HistogramVar *histogram(const std::string& name);
	SummaryVar *summary(const std::string& name);

	// filter api
	bool client_end(SubTask *task, RPCModuleData& data) override;
	bool server_end(SubTask *task, RPCModuleData& data) override;

public:
	RPCMetricsFilter();

protected:
	void reduce(std::unordered_map<std::string, RPCVar *>& out);

protected:
	std::mutex mutex;
	std::set<std::string> var_names;
};

class RPCMetricsPull : public RPCMetricsFilter
{
public:
	bool init(unsigned short port); // pull port for Prometheus
	void deinit();

public:
	RPCMetricsPull();

private:
	SubTask *create(RPCModuleData& data) override
	{
		return WFTaskFactory::create_empty_task();
	}

	bool filter(RPCModuleData& data) override
	{
		return false; // will not collect by policy.collect(data);
	}

private:
	class Collector : public RPCVarCollector
	{
	public:
		Collector(std::string& output) : report_output(output) { }

		void collect_gauge(RPCVar *gauge, double data) override;

		void collect_counter_each(RPCVar *counter, const std::string& label,
								  double data) override;

		void collect_histogram_begin(RPCVar *histogram) override { }
		void collect_histogram_each(RPCVar *histogram,
									double bucket_boudary,
									size_t current_count) override;
		void collect_histogram_end(RPCVar *histogram, double sum,
								   size_t count) override;

		void collect_summary_begin(RPCVar *summary) override { }
		void collect_summary_each(RPCVar *summary, double quantile,
								  double quantile_out) override;
		void collect_summary_end(RPCVar *summary, double sum,
								 size_t count) override;

	private:
		std::string& report_output;
	};

private:
	bool expose();
	void pull(WFHttpTask *task);
	std::string report_output;
	Collector collector;
	WFHttpServer server;
};

class RPCFilterPolicy
{
public:
	RPCFilterPolicy(size_t report_threshold,
					size_t report_interval_msec) :
		report_threshold(report_threshold),
		report_interval(report_interval_msec),
		last_report_timestamp(0)
	{ }
	
	void set_report_threshold(size_t threshold)
	{
		if (threshold <= 0)
			threshold = 1;
		this->report_threshold = threshold;
	}

	void set_report_interval(int msec)
	{
		if (msec <= 0)
			msec = 1;
		this->report_interval = msec;
	}

	bool report(size_t count);

private:
	size_t report_threshold; // metrics to report at most
	size_t report_interval;
	long long last_report_timestamp;
};

class RPCMetricsOTel : public RPCMetricsFilter
{
public:
	void set_report_threshold(size_t threshold)
	{
		this->policy.set_report_threshold(threshold);
	}

	void set_report_interval(int msec)
	{
		this->policy.set_report_interval(msec);
	}

	// for client level attributes, such as ProviderID
	void add_attributes(const std::string& key, const std::string& value);
	size_t clear_attributes();

public:
	RPCMetricsOTel(const std::string& url);

	RPCMetricsOTel(const std::string& url, unsigned int redirect_max,
				   unsigned int retry_max, size_t report_threshold,
				   size_t report_interval);

private:
	SubTask *create(RPCModuleData& data) override;
	bool filter(RPCModuleData& data) override
	{
		return this->policy.report(++this->report_counts);
	}

private:
	class Collector : public RPCVarCollector
	{
	public:
		Collector() { }
		virtual ~Collector();

		void set_current_message(google::protobuf::Message *var_msg)
		{
			this->current_msg = var_msg;
		}

		void set_current_nano(unsigned long long ns)
		{
			this->current_timestamp_nano = ns;
		}

		void collect_gauge(RPCVar *gauge, double data) override;

		void collect_counter_each(RPCVar *counter, const std::string& label,
								  double data) override;

		void collect_histogram_begin(RPCVar *histogram) override;
		void collect_histogram_each(RPCVar *histogram,
									double bucket_boudary,
									size_t current_count) override;
		void collect_histogram_end(RPCVar *histogram, double sum,
								   size_t count) override;

		void collect_summary_begin(RPCVar *summary) override;
		void collect_summary_each(RPCVar *summary, double quantile,
								  double quantile_out) override;
		void collect_summary_end(RPCVar *summary, double sum,
								 size_t count) override;

	private:
		void add_counter_label(const std::string& label);

	private:
		using LABEL_MAP = std::map<std::string, std::string>;
		google::protobuf::Message *current_msg;
		unsigned long long current_timestamp_nano;
		std::map<std::string, LABEL_MAP *> label_map;
	};

private:
	bool expose(google::protobuf::Message *metrics);

private:
	std::string url;
	int redirect_max;
	int retry_max;
	Collector collector;
	RPCFilterPolicy policy;
	std::atomic<size_t> report_counts;
	std::mutex mutex;
	std::map<std::string, std::string> attributes;
};

} // end namespace srpc

#endif

