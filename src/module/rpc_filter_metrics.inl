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

#include <string>
#include <set>
#include <vector>
#include <mutex>
#include <chrono>
#include "rpc_basic.h"

namespace srpc
{
static constexpr size_t			METRICS_DEFAULT_MAX_AGE		= 60;
static constexpr size_t			METRICS_DEFAULT_AGE_BUCKET	= 5;
static constexpr const char	   *METRICS_OTLP_METRICS_PATH	= "/v1/metrics";
static constexpr const char	   *METRICS_PULL_METRICS_PATH	= "/metrics";

template<typename TYPE>
GaugeVar<TYPE> *RPCMetricsFilter::create_gauge(const std::string& name,
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

	GaugeVar<TYPE> *gauge = new GaugeVar<TYPE>(name, help);
	RPCVarLocal::get_instance()->add(name, gauge);
	return gauge;
}

template<typename TYPE>
CounterVar<TYPE> *RPCMetricsFilter::create_counter(const std::string& name,
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

	CounterVar<TYPE> *counter = new CounterVar<TYPE>(name, help);
	RPCVarLocal::get_instance()->add(name, counter);
	return counter;
}

template<typename TYPE>
HistogramVar<TYPE> *RPCMetricsFilter::create_histogram(const std::string& name,
													   const std::string& help,
													   const std::vector<TYPE>& bucket)
{
	this->mutex.lock();
	const auto it = var_names.insert(name);
	this->mutex.unlock();

	if (!it.second)
	{
		errno = EEXIST;
		return NULL;
	}

	HistogramVar<TYPE> *histogram = new HistogramVar<TYPE>(name, help, bucket);
	RPCVarLocal::get_instance()->add(name, histogram);
	return histogram;
}

template<typename TYPE>
SummaryVar<TYPE> *RPCMetricsFilter::create_summary(const std::string& name,
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

	SummaryVar<TYPE> *summary = new SummaryVar<TYPE>(name, help, quantile,
								std::chrono::seconds(METRICS_DEFAULT_MAX_AGE),
								METRICS_DEFAULT_AGE_BUCKET);

	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

template<typename TYPE>
SummaryVar<TYPE> *RPCMetricsFilter::create_summary(const std::string& name,
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

	SummaryVar<TYPE> *summary = new SummaryVar<TYPE>(name, help, quantile,
													 max_age, age_bucket);
	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

} // end namespace srpc

