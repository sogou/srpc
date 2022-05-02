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
#include "workflow/WFTask.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHttpServer.h"
#include "rpc_basic.h"
#include "rpc_var_factory.h"
#include "rpc_module_metrics.h"

namespace srpc
{

class RPCMetricsPull : public RPCFilter
{
public:
	bool init(short port); // pull port for Prometheus
	void deinit();

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
public:
	RPCMetricsPull();

private:
	void pull(WFHttpTask *task);
	bool expose();
	std::mutex mutex;
	std::set<std::string> var_names;
	WFHttpServer server;
	std::string report_output;

	SubTask *create(RPCModuleData& data) override
	{
		return WFTaskFactory::create_empty_task();
	}

	bool filter(RPCModuleData& data) override
	{
		return false; // will not collect by filter_policy.collect(data);
	}

private:
	template<typename TYPE>
	class StringGaugeVar : public GaugeVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};

	template<typename TYPE>
	class StringCounterVar : public CounterVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};

	template<typename TYPE>
	class StringSummaryVar : public SummaryVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};

	template<typename TYPE>
	class StringHistogramVar : public HistogramVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};
};

class RPCMetricsOpenTelemetry : public RPCFilter
{
public:
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

public:
	RPCMetricsOpenTelemetry(/* policy params */) :
		RPCFilter(RPCModuleTypeMetrics)
	{ }

private:
	bool expose();
	std::mutex mutex;
	std::set<std::string> var_names;

	//TODO: refer to otel span
	SubTask *create(RPCModuleData& data) override
	{
		return WFTaskFactory::create_empty_task();
	}

	bool filter(RPCModuleData& data) override
	{
		return false;
//		return this->filter_policy.collect(data);
	}

private:
	template<typename TYPE>
	class ProtobufGaugeVar : public GaugeVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};

	template<typename TYPE>
	class ProtobufCounterVar : public CounterVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};

	template<typename TYPE>
	class ProtobufSummaryVar : public SummaryVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};

	template<typename TYPE>
	class ProtobufHistogramVar : public HistogramVar<TYPE>
	{
	public:
		void collect(void *output) override;
	};
};

/*
class RPCOTelFilter : public RPCFilter, public RPCVarCollector
{
public:
	 bool collect(const RPCVar *var) override;
};
*/

} // end namespace srpc

#include "rpc_filter_metrics.inl"

#endif

