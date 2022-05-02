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

template<typename TYPE>
void RPCMetricsPull::StringGaugeVar<TYPE>::collect(void *output)
{
	std::string *str = (std::string *)output;
	*str = this->name + " " + this->data_str() + "\n";
}

template<typename TYPE>
void RPCMetricsPull::StringCounterVar<TYPE>::collect(void *output)
{
	std::string *str = (std::string *)output;

	for (auto it = this->data.begin(); it != this->data.end(); it++)
	{
		*str += this->name + "{" + it->first + "}" + " " +
			   it->second->data_str() + "\n";
	}
}

template<typename TYPE>
void RPCMetricsPull::StringSummaryVar<TYPE>::collect(void *output)
{
	std::string *str = (std::string *)output;

	for (size_t i = 0; i < this->quantiles.size(); i++)
	{
		*str += this->name + "{quantile=\"" +
			   std::to_string(this->quantiles[i].quantile) + "\"} ";

		if (this->quantile_out[i] ==
			std::numeric_limits<TYPE>::quiet_NaN())
		{
			*str += "NaN";
		}
		else
		{
			*str += std::to_string(this->quantile_out[i] /
								   this->available_count[i]);
		}
		*str += "\n";
	}

	*str += this->name + "_sum " + std::to_string(this->sum) + "\n";
	*str += this->name + "_count " + std::to_string(this->count) + "\n";

	this->quantile_out.clear();
	this->available_count.clear();
}

template<typename TYPE>
void RPCMetricsPull::StringHistogramVar<TYPE>::collect(void *output)
{
	std::string *str = (std::string *)output;
	size_t i = 0;
	size_t current = 0;

	for (; i < this->bucket_boundaries.size(); i++)
	{
		current += this->bucket_counts[i];
		*str += this->name + "_bucket{le=\"" +
				std::to_string(this->bucket_boundaries[i]) + "\"} " +
				std::to_string(current) + "\n";
	}

	current += this->bucket_counts[i];
	*str += this->name + "_bucket{le=\"+Inf\"} " +
			std::to_string(current) + "\n";

	*str += this->name + "_sum " + std::to_string(this->sum) + "\n";
	*str += this->name + "_count " + std::to_string(this->count) + "\n";
}

template<typename TYPE>
GaugeVar<TYPE> *RPCMetricsPull::create_gauge(const std::string& name,
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

	GaugeVar<TYPE> *gauge = new StringGaugeVar<TYPE>(name, help);
	RPCVarLocal::get_instance()->add(name, gauge);
	return gauge;
}

template<typename TYPE>
CounterVar<TYPE> *RPCMetricsPull::create_counter(const std::string& name,
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

	CounterVar<TYPE> *counter = new StringCounterVar<TYPE>(name, help);
	RPCVarLocal::get_instance()->add(name, counter);
	return counter;
}

template<typename TYPE>
HistogramVar<TYPE> *RPCMetricsPull::create_histogram(const std::string& name,
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

	HistogramVar<TYPE> *histogram = new StringHistogramVar<TYPE>(name,
																 help,
																 bucket);
	RPCVarLocal::get_instance()->add(name, histogram);
	return histogram;
}

template<typename TYPE>
SummaryVar<TYPE> *RPCMetricsPull::create_summary(const std::string& name,
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

	SummaryVar<TYPE> *summary = new StringSummaryVar<TYPE>(name,
														   help,
														   quantile,
								std::chrono::seconds(METRICS_DEFAULT_MAX_AGE),
								METRICS_DEFAULT_AGE_BUCKET);

	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

template<typename TYPE>
SummaryVar<TYPE> *RPCMetricsPull::create_summary(const std::string& name,
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

	SummaryVar<TYPE> *summary = new StringSummaryVar<TYPE>(name, help,
														   quantile, max_age,
														   age_bucket);
	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

template<typename TYPE>
GaugeVar<TYPE> *RPCMetricsOpenTelemetry::create_gauge(const std::string& name,
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

	GaugeVar<TYPE> *gauge = new ProtobufGaugeVar<TYPE>(name, help);
	RPCVarLocal::get_instance()->add(name, gauge);
	return gauge;
}

template<typename TYPE>
CounterVar<TYPE> *RPCMetricsOpenTelemetry::create_counter(const std::string& name,
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

	CounterVar<TYPE> *counter = new ProtobufCounterVar<TYPE>(name, help);
	RPCVarLocal::get_instance()->add(name, counter);
	return counter;
}

template<typename TYPE>
HistogramVar<TYPE> *RPCMetricsOpenTelemetry::create_histogram(const std::string& name,
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

	HistogramVar<TYPE> *histogram = new ProtobufHistogramVar<TYPE>(name,
																   help,
																   bucket);
	RPCVarLocal::get_instance()->add(name, histogram);
	return histogram;
}

template<typename TYPE>
SummaryVar<TYPE> *RPCMetricsOpenTelemetry::create_summary(const std::string& name,
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

	SummaryVar<TYPE> *summary = new ProtobufSummaryVar<TYPE>(name, help,
															 quantile,
								std::chrono::seconds(METRICS_DEFAULT_MAX_AGE),
								METRICS_DEFAULT_AGE_BUCKET);

	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

template<typename TYPE>
SummaryVar<TYPE> *RPCMetricsOpenTelemetry::create_summary(const std::string& name,
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

	SummaryVar<TYPE> *summary = new ProtobufSummaryVar<TYPE>(name, help,
															 quantile, max_age,
															 age_bucket);
	RPCVarLocal::get_instance()->add(name, summary);
	return summary;
}

} // end namespace srpc

