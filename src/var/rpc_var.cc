/*
  Copyright (c) 2021 Sogou, Inc.

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

#include <mutex>
#include <string>
#include <cfloat>
#include <vector>
#include <unordered_map>
#include "rpc_var.h"

namespace srpc
{

GaugeVar *RPCVarFactory::gauge(const std::string& name)
{
	return static_cast<GaugeVar *>(RPCVarFactory::var(name));
}

CounterVar *RPCVarFactory::counter(const std::string& name)
{
	return static_cast<CounterVar *>(RPCVarFactory::var(name));
}

HistogramVar *RPCVarFactory::histogram(const std::string& name)
{
	return static_cast<HistogramVar *>(RPCVarFactory::var(name));
}

SummaryVar *RPCVarFactory::summary(const std::string& name)
{
	return static_cast<SummaryVar *>(RPCVarFactory::var(name));
}

RPCVar *RPCVarFactory::var(const std::string& name)
{
	RPCVar *var;
	RPCVar *new_var;
	RPCVarLocal *local = RPCVarLocal::get_instance();
	auto it = local->vars.find(name);
	if (it != local->vars.end())
		return it->second;

	var = RPCVarGlobal::get_instance()->find(name);

	if (var)
	{
		new_var = var->create(false);
		local->add(name, new_var);
		return new_var;
	}

	return NULL;
}

// a~z, A~Z, _
bool RPCVarFactory::check_name_format(const std::string& name)
{
	for (size_t i = 0; i < name.length(); i++)
	{
		if ((name.at(i) < 65 || name.at(i) > 90) &&
			(name.at(i) < 97 || name.at(i) > 122) &&
			name.at(i) != 95)
		return false;
	}

	return true;
}


void RPCVar::format_name()
{
	//TODO: change aaa.bbb AAA.BBB to aaa_bbb
}

RPCVarLocal::~RPCVarLocal()
{
	RPCVarGlobal *global_var = RPCVarGlobal::get_instance();

	global_var->remove(this);
	global_var->dup(this->vars);

	for (auto it = this->vars.begin(); it != this->vars.end(); it++)
		delete it->second;
}

RPCVarGlobal::~RPCVarGlobal()
{
	this->finished = true;

	for (auto& local : this->local_vars)
		delete local;
}

void RPCVarGlobal::dup(const std::unordered_map<std::string, RPCVar *>& vars)
{
	if (this->finished == true)
		return;

	RPCVarLocal *local;

	this->mutex.lock();
	if (this->local_vars.empty())
		local = NULL;
	else
		local = this->local_vars[0];

	this->mutex.unlock();

	if (local == NULL)
		local = new RPCVarLocal();

	local->mutex.lock();
	std::unordered_map<std::string, RPCVar*>& local_var = local->vars;

	for (auto it = vars.begin(); it != vars.end(); it++)
	{
		if (local_var.find(it->first) == local_var.end())
		{
			local_var.insert(std::make_pair(it->first,
											it->second->create(true)));
		}
		else
		{
			local_var[it->first]->reduce(it->second->get_data(),
										 it->second->get_size());
		}
	}

	local->mutex.unlock();
}

void RPCVarGlobal::remove(const RPCVarLocal *var)
{
	this->mutex.lock();
	for (size_t i = 0; i < this->local_vars.size(); i++)
	{
		if (this->local_vars[i] == var)
		{
			for (size_t j = i; j < this->local_vars.size() - 1; j++)
				this->local_vars[j] = this->local_vars[j + 1];

			break;
		}
	}

	this->local_vars.resize(this->local_vars.size() - 1);
	this->mutex.unlock();
}

RPCVar *RPCVarGlobal::find(const std::string& name)
{
	std::unordered_map<std::string, RPCVar*>::iterator it;
	RPCVarGlobal *global_var = RPCVarGlobal::get_instance();
	RPCVar *ret = NULL;
	RPCVarLocal *local;

	global_var->mutex.lock();
	for (size_t i = 0; i < global_var->local_vars.size() && !ret; i++)
	{
		local = global_var->local_vars[i];
		local->mutex.lock();

		for (it = local->vars.begin(); it != local->vars.end() && !ret; it++)
		{
			if (!name.compare(it->second->get_name()))
				ret = it->second;
		}
		local->mutex.unlock();
	}

	global_var->mutex.unlock();
	return ret;
}

///////////// var impl

RPCVar *GaugeVar::create(bool with_data)
{
	GaugeVar *var = new GaugeVar(this->name, this->help);

	if (with_data)
		var->data = this->data;

	return var;
}

CounterVar::~CounterVar()
{
	for (auto it = this->data.begin(); it != this->data.end(); it++)
		delete it->second;
}

// Caution :
//    make sure local->mutex.lock() before CounterVar::create(true)
RPCVar *CounterVar::create(bool with_data)
{
	CounterVar *var = new CounterVar(this->name, this->help);

	if (with_data)
	{
		for (auto it = this->data.begin(); it != this->data.end(); it++)
		{
			var->data.insert(std::make_pair(it->first,
							(GaugeVar *)it->second->create(true)));
		}
	}

	return var;
}

bool CounterVar::label_to_str(const LABEL_MAP& labels, std::string& str)
{
	for (auto it = labels.begin(); it != labels.end(); it++)
	{
		if (it != labels.begin())
			str += ",";
		//TODO: check label name regex is "[a-zA-Z_:][a-zA-Z0-9_:]*"
		str += it->first + "=\"" + it->second + "\"";
	}

	return true;
}

// [deprecate]
//    This cannot guarantee the GaugeVar still exists
//    because global will counter->reset() and delete the internal GaugeVar
GaugeVar *CounterVar::add(const LABEL_MAP& labels)
{
	std::string label_str;
	GaugeVar *var;

	if (!this->label_to_str(labels, label_str))
		return NULL;

	auto it = this->data.find(label_str);

	if (it == this->data.end())
	{
		var = new GaugeVar(label_str, "");
		this->data.insert(std::make_pair(label_str, var));
	}
	else
		var = it->second;

	return var;
}

void CounterVar::increase(const LABEL_MAP& labels)
{
	std::string label_str;
	GaugeVar *var;

	if (!this->label_to_str(labels, label_str))
		return;

	RPCVarLocal *local = RPCVarLocal::get_instance();
	local->mutex.lock(); // against reset() and delete GaugeVar

	auto it = this->data.find(label_str);

	if (it == this->data.end())
	{
		var = new GaugeVar(label_str, "");
		this->data.insert(std::make_pair(label_str, var));
	}
	else
		var = it->second;

	var->increase();
	local->mutex.unlock();

	return;
}

// Caution :
//    make sure local->mutex.lock() before CounterVar::reduce()
bool CounterVar::reduce(const void *ptr, size_t)
{
	std::unordered_map<std::string, GaugeVar *> *data;
	data = (std::unordered_map<std::string, GaugeVar *> *)ptr;

	for (auto it = data->begin(); it != data->end(); it++)
	{
		auto my_it = this->data.find(it->first);

		if (my_it == this->data.end())
		{
			GaugeVar *var = (GaugeVar *)it->second->create(true);
			this->data.insert(std::make_pair(it->first, var));
		}
		else
			my_it->second->reduce(it->second->get_data(),
								  it->second->get_size());
	}

	return true;
}

void CounterVar::collect(RPCVarCollector *collector)
{
	for (auto it = this->data.begin(); it != this->data.end(); it++)
		collector->collect_counter_each(this, it->first, it->second->get());
}

void CounterVar::reset()
{
	for (auto it = this->data.begin(); it != this->data.end(); it++)
		delete it->second;

	this->data.clear();
}

void HistogramVar::observe(double value)
{
	size_t i = 0;

	for (; i < this->bucket_boundaries.size(); i++)
	{
		if (value <= this->bucket_boundaries[i])
			break;
	}

	this->bucket_counts[i]++;
	this->sum += value;
	this->count++;
}

HistogramVar::HistogramVar(const std::string& name, const std::string& help,
						   const std::vector<double>& bucket) :
	RPCVar(name, help, VAR_HISTOGRAM),
	bucket_boundaries(bucket),
	bucket_counts(bucket.size() + 1)
{
	this->sum = 0;
	this->count = 0;
}

RPCVar *HistogramVar::create(bool with_data)
{
	HistogramVar *var = new HistogramVar(this->name, this->help,
										 this->bucket_boundaries);
	if (with_data)
	{
		var->bucket_counts = this->bucket_counts;
		var->sum = this->sum;
		var->count = this->count;
	}

	return var;
}

bool HistogramVar::observe_multi(const std::vector<size_t>& multi, double sum)
{
	if (multi.size() != this->bucket_counts.size())
		return false;

	for (size_t i = 0; i < multi.size(); i ++)
	{
		this->bucket_counts[i] += multi[i];
		this->count += multi[i];
	}
	this->sum += sum;

	return true;
}

bool HistogramVar::reduce(const void *ptr, size_t sz)
{
	if (sz != this->bucket_boundaries.size() + 1)
		return false;

	const HistogramVar *data = (const HistogramVar *)ptr;
	const std::vector<size_t> *src_bucket_counts = data->get_bucket_counts();

	for (size_t i = 0; i < sz; i++)
		this->bucket_counts[i] += (*src_bucket_counts)[i];

	this->sum += data->get_sum();
	this->count += data->get_count();

	return true;
}

void HistogramVar::collect(RPCVarCollector *collector)
{
	size_t i = 0;
	size_t current = 0;

	collector->collect_histogram_begin(this);
	for (; i < this->bucket_boundaries.size(); i++)
	{
		current += this->bucket_counts[i];
		collector->collect_histogram_each(this, this->bucket_boundaries[i],
										  current);
	}

	current += this->bucket_counts[i];
	collector->collect_histogram_each(this, DBL_MAX, current);
	collector->collect_histogram_end(this, this->sum, this->count);
}

void HistogramVar::reset() {
	for (size_t i = 0; i < this->bucket_boundaries.size(); i++)
		this->bucket_counts[i] = 0;
	this->sum = 0;
	this->count = 0;
}

SummaryVar::SummaryVar(const std::string& name, const std::string& help,
					   const std::vector<struct Quantile>& quantile,
					   const std::chrono::milliseconds max_age, int age_bucket) :
	RPCVar(name, help, VAR_SUMMARY),
	quantiles(quantile),
	quantile_values(&this->quantiles, max_age, age_bucket)
{
	this->sum = 0;
	this->count = 0;
	this->max_age = max_age;
	this->age_buckets = age_bucket;
	this->quantile_size = this->quantiles.size(); // for output
	if (this->quantiles[this->quantile_size - 1].quantile != 1.0)
	{
		struct Quantile q(1.0, 0.1);
		this->quantiles.push_back(q);
	}
	this->quantile_out.resize(this->quantiles.size(), 0);
}

RPCVar *SummaryVar::create(bool with_data)
{
	SummaryVar *var = new SummaryVar(this->name, this->help,
									 this->quantiles, this->max_age,
									 this->age_buckets);
	if (with_data)
	{
		var->sum = this->sum;
		var->count = this->count;
		var->quantile_values = this->quantile_values;
		var->quantile_size = this->quantile_size;
		var->quantile_out = this->quantile_out;
	}

	return var;
}

void SummaryVar::observe(double value)
{
	this->quantile_values.insert(value);
}

bool SummaryVar::reduce(const void *ptr, size_t sz)
{
	if (sz != this->quantiles.size())
		return false;

	SummaryVar *data = (SummaryVar *)ptr;

	TimeWindowQuantiles<double> *src = data->get_quantile_values();
	double get_val;
	size_t src_count = 0;
	double *src_value = new double[sz]();
	double src_sum = src->get_sum();

	for (size_t i = 0; i < sz; i++)
	{
		src_count = src->get(this->quantiles[i].quantile, &get_val);
		src_value[i] = get_val;
	}

	double pilot;
	size_t cnt;
	size_t idx;
	double range;
	double count = 0;
	double value = 0;
	size_t src_idx = 0;
	size_t dst_idx = 0;
	size_t total = this->count + src_count;
	double *out = new double[sz]();

	for (size_t i = 0; i < sz; i++)
	{
		pilot = this->quantiles[i].quantile * total;

		while (count < pilot && src_idx < sz && dst_idx < sz)
		{
			if (this->quantile_out[dst_idx] <= src_value[src_idx])
			{
				value = this->quantile_out[dst_idx];
				idx = dst_idx;
				cnt = this->count;
				dst_idx++;
			}
			else
			{
				value = src_value[src_idx];
				idx = src_idx;
				cnt = src_count;
				src_idx++;
			}

			if (idx == 0)
				range = this->quantiles[0].quantile;
			else
				range = this->quantiles[idx].quantile -
						this->quantiles[idx - 1].quantile;

			count += cnt * range;
		}

		if (count >= pilot)
			out[i] = value;
		else if (src_idx < sz)
			out[i] = src_value[i];
		else
			out[i] = this->quantile_out[i];
	}

	for (size_t i = 0; i < sz; i++)
		this->quantile_out[i] = out[i];

	this->count = total;
	this->sum += src_sum;

	delete[] out;
	delete[] src_value;

	return true;
}

void SummaryVar::collect(RPCVarCollector *collector)
{
	collector->collect_summary_begin(this);

	for (size_t i = 0; i < this->quantile_size; i++)
	{
		collector->collect_summary_each(this, this->quantiles[i].quantile,
										this->quantile_out[i]);
	}

	collector->collect_summary_end(this, this->sum, this->count);
	this->quantile_out.clear();
}

TimedGaugeVar::TimedGaugeVar(const std::string& name, const std::string& help,
							 Clock::duration duration, size_t bucket_num) :
	GaugeVar(name, help),
	RPCTimeWindow<GaugeVar>(duration, bucket_num)
{
	for (size_t i = 0; i < bucket_num; i ++)
		this->data_bucket.push_back(GaugeVar(name, help));
}

void TimedGaugeVar::increase()
{
	this->rotate();

	for (auto& bucket : this->data_bucket)
		bucket.increase();
}

const void *TimedGaugeVar::get_data()
{
	GaugeVar& bucket = this->rotate();
	return bucket.get_data();
}

RPCVar *TimedGaugeVar::create(bool with_data)
{
	GaugeVar& current_bucket = this->rotate();
	RPCVar *var;

	if (with_data) // for reduce
	{
		GaugeVar *gauge = new GaugeVar(this->get_name(), this->get_help());
		gauge->set(*(double *)current_bucket.get_data());
		var = gauge;
	}
	else // for dup into other threads
	{
		TimedGaugeVar *timed_gauge = new TimedGaugeVar(this->get_name(),
													   this->get_help(),
													   this->duration,
													   this->bucket_num);
		var = timed_gauge;
	}

	return var;
}

} // end namespace srpc

