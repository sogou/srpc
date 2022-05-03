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
#include <vector>
#include <mutex>
#include <chrono>
#include <google/protobuf/message.h>
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHttpServer.h"
#include "rpc_basic.h"
#include "rpc_var_factory.h"
#include "rpc_module_metrics.h"

namespace srpc
{

class RPCMetricsFilter : public RPCFilter
{
public:
	RPCMetricsFilter() : RPCFilter(RPCModuleTypeMetrics) {}

	template<typename TYPE>
	GaugeVar<TYPE> *create_gauge(const std::string& name,
								 const std::string& help);

	template<typename TYPE>
	CounterVar<TYPE> *create_counter(const std::string& name,
									 const std::string& help);

	template<typename TYPE>
	HistogramVar<TYPE> *create_histogram(const std::string& name,
										 const std::string& help,
										 const std::vector<TYPE>& bucket);

	template<typename TYPE>
	SummaryVar<TYPE> *create_summary(const std::string& name,
									 const std::string& help,
									 const std::vector<struct Quantile>& quantile);

	template<typename TYPE>
	SummaryVar<TYPE> *create_summary(const std::string& name,
									 const std::string& help,
									 const std::vector<struct Quantile>& quantile,
									 const std::chrono::milliseconds max_age,
									 int age_bucket);
private:
	virtual bool expose() = 0;

protected:
	std::mutex mutex;
	std::set<std::string> var_names;
};

class RPCMetricsPull : public RPCMetricsFilter
{
public:
	bool init(short port); // pull port for Prometheus
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
		return false; // will not collect by filter_policy.collect(data);
	}

private:
	class Collector : public RPCVarCollector
	{
	public:
		Collector(std::string& output) : report_output(output) { }

		void collect_gauge(RPCVar *gauge, int64_t data) override;
		void collect_gauge(RPCVar *gauge, double data) override;

		void collect_counter_each(RPCVar *counter, const std::string& label,
								  int64_t data) override;
		void collect_counter_each(RPCVar *counter, const std::string& label,
								  double data) override;

		void collect_histogram_each(RPCVar *histogram,
									int64_t bucket_boudaries,
									size_t current_count)override;
		void collect_histogram_sum(RPCVar *histogram, int64_t sum,
								   size_t count) override;

		void collect_histogram_each(RPCVar *histogram,
									double bucket_boudaries,
									size_t current_count) override;
		void collect_histogram_sum(RPCVar *histogram, double sum,
								   size_t count) override;

		void collect_summary_each(RPCVar *summary, double quantile,
								  int64_t quantile_out,
								  size_t available_count) override;
		void collect_summary_sum(RPCVar *summary, int64_t sum,
								 size_t count) override;
		void collect_summary_each(RPCVar *summary, double quantile,
								  double quantile_out,
								  size_t available_count) override;
		void collect_summary_sum(RPCVar *summary, double sum,
								 size_t count) override;

	private:
		std::string& report_output;
	};

private:
	bool expose() override;
	void pull(WFHttpTask *task);
	std::string report_output;
	Collector collector;
	WFHttpServer server;
};

class RPCMetricsOTel : public RPCMetricsFilter
{
public:
	RPCMetricsOTel(/* policy params */);
	virtual ~RPCMetricsOTel()
	{
		delete this->report_req;
	}

private:
	SubTask *create(RPCModuleData& data) override
	{
		return WFTaskFactory::create_empty_task();
	}

	bool filter(RPCModuleData& data) override
	{
		return false; // TODO: this->filter_policy.collect(data);
	}

private:
	class Collector : public RPCVarCollector
	{
	public:
		Collector() { }
		void set_report_req(google::protobuf::Message *req)
		{
			this->report_req = req;
		}

		void collect_gauge(RPCVar *gauge, int64_t data) override;
		void collect_gauge(RPCVar *gauge, double data) override;

		void collect_counter_each(RPCVar *counter, const std::string& label,
								  int64_t data) override;
		void collect_counter_each(RPCVar *counter, const std::string& label,
								  double data) override;

		void collect_histogram_each(RPCVar *histogram,
									int64_t bucket_boudary,
									size_t current_count)override;
		void collect_histogram_sum(RPCVar *histogram, int64_t sum,
								   size_t count) override;

		void collect_histogram_each(RPCVar *histogram,
									double bucket_boudary,
									size_t current_count) override;
		void collect_histogram_sum(RPCVar *histogram, double sum,
								   size_t count) override;

		void collect_summary_each(RPCVar *summary, double quantile,
								  int64_t quantile_out,
								  size_t available_count) override;
		void collect_summary_sum(RPCVar *summary, int64_t sum,
								 size_t count) override;
		void collect_summary_each(RPCVar *summary, double quantile,
								  double quantile_out,
								  size_t available_count) override;
		void collect_summary_sum(RPCVar *summary, double sum,
								 size_t count) override;

	private:
		google::protobuf::Message *report_req;
	};

private:
	bool expose() override;
	Collector collector;
	google::protobuf::Message *report_req;
};

} // end namespace srpc

#include "rpc_filter_metrics.inl"

#endif

