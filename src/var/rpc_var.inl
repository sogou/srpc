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

#include <utility>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include "time_window_quantiles.h"

namespace srpc
{

template<typename TYPE>
class GaugeVar : public RPCVar
{
public:
	void increase() { ++this->data; }
	void decrease() { --this->data; }
	size_t get_size() const override { return sizeof(TYPE); }
	const void *get_data() const override { return &this->data; }

	virtual TYPE get() const { return this->data; }
	virtual void set(TYPE var) { this->data = var; }

	bool reduce(const void *ptr, size_t sz) override
	{
		if (sz != sizeof(TYPE))
			return false;

		TYPE *data = (TYPE *)ptr;
		this->data += *data;
//		fprintf(stderr, "reduce data=%d *ptr=%d\n", this->data, *(int *)ptr);
		return true;
	}

	void collect(RPCVarCollector *collector) override
	{
		collector->collect_gauge(this, this->data);
	}

public:
	virtual void init() { this->data = 0; }

	GaugeVar(const std::string& name, const std::string& help) :
		RPCVar(name, help, VAR_GAUGE)
	{
		this->init();
	}

	RPCVar *create(bool with_data) override
	{
		GaugeVar *var =  new GaugeVar<TYPE>(this->name, this->help);

		if (with_data)
			var->data = this->data;

		return var;
	}

	std::string data_str()
	{
		return std::to_string(this->data);
	}

	virtual ~GaugeVar() {}

	GaugeVar(GaugeVar<TYPE>&& move) = default;
	GaugeVar& operator=(GaugeVar<TYPE>&& move) = default;

private:
	TYPE data;
};

template<typename TYPE>
class CounterVar : public RPCVar
{
public:
	using LABEL_MAP = std::map<std::string, std::string>;
	GaugeVar<TYPE> *add(const LABEL_MAP& labels);

	bool reduce(const void *ptr, size_t sz) override;
	void collect(RPCVarCollector *collector) override;

	size_t get_size() const override { return this->data.size(); }
	const void *get_data() const override { return &this->data; }

	static bool label_to_str(const LABEL_MAP& labels, std::string& str);

public:
	RPCVar *create(bool with_data) override
	{
		CounterVar<TYPE> *var = new CounterVar<TYPE>(this->name, this->help);

		if (with_data)
		{
			for (auto it = this->data.begin();
				 it != this->data.end(); it++)
			{
				this->data.insert(std::make_pair(it->first,
							   (GaugeVar<TYPE> *)it->second->create(true)));
			}
		}

		return var;
	}

	CounterVar(const std::string& name, const std::string& help) :
		RPCVar(name, help, VAR_COUNTER)
	{
	}

	~CounterVar()
	{
		for (auto it = this->data.begin();
			 it != this->data.end(); it++)
		{
			delete it->second;
		}
	}

private:
	std::unordered_map<std::string, GaugeVar<TYPE> *> data;
};

template<typename TYPE>
class HistogramVar : public RPCVar
{
public:
	void observe(const TYPE value);
	bool observe_multi(const std::vector<TYPE>& multi, const TYPE sum);

	bool reduce(const void *ptr, size_t sz) override;
	void collect(RPCVarCollector *collector) override;

	size_t get_size() const override { return this->bucket_counts.size(); }
	const void *get_data() const override { return this; }

	TYPE get_sum() const { return this->sum; }
	size_t get_count() const { return this->count; }
	const std::vector<size_t> *get_bucket_counts() const
	{
		return &this->bucket_counts;
	}

public:
	RPCVar *create(bool with_data) override
	{
		HistogramVar *var = new HistogramVar<TYPE>(this->name, this->help,
												   this->bucket_boundaries);
		if (with_data)
		{
			var->bucket_counts = this->bucket_counts;
			var->sum = this->sum;
			var->count = this->count;
		}

		return var;
	}

	HistogramVar(const std::string& name, const std::string& help,
				 const std::vector<TYPE>& bucket) :
		RPCVar(name, help, VAR_HISTOGRAM),
		bucket_boundaries(bucket),
		bucket_counts(bucket.size() + 1)
	{
		this->init();
	}

	virtual void init()
	{
		this->sum = 0;
		this->count = 0;
	}

private:
	std::vector<TYPE> bucket_boundaries;
	std::vector<size_t> bucket_counts;
	TYPE sum;
	size_t count;
};

template<typename TYPE>
class SummaryVar : public RPCVar
{
public:
	void observe(const TYPE value);

	bool reduce(const void *ptr, size_t sz) override;
	void collect(RPCVarCollector *collector) override;

	size_t get_size() const override { return this->quantiles.size(); }
	const void *get_data() const override { return this; }

	TYPE get_sum() const { return this->sum; }
	size_t get_count() const { return this->count; }
	TimeWindowQuantiles<TYPE> *get_quantile_values()
	{
		return &this->quantile_values;
	}

public:
	RPCVar *create(bool with_data) override
	{
		SummaryVar<TYPE> *var = new SummaryVar<TYPE>(this->name,
													 this->help,
													 this->quantiles,
													 this->max_age,
													 this->age_buckets);
		if (with_data)
		{
			var->sum = this->sum;
			var->count = this->count;
			var->quantile_values = this->quantile_values;
		}

		return var;
	}

	SummaryVar(const std::string& name, const std::string& help,
			   const std::vector<struct Quantile>& quantile,
			   const std::chrono::milliseconds max_age, int age_bucket) :
		RPCVar(name, help, VAR_SUMMARY),
		quantiles(quantile),
		quantile_values(&this->quantiles, max_age, age_bucket)
	{
		this->max_age = max_age;
		this->age_buckets = age_bucket;
		this->quantile_out.resize(quantile.size(), 0);
		this->available_count.resize(quantile.size(), 0);
		this->init();
	}

	virtual void init()
	{
		this->sum = 0;
		this->count = 0;
	}

private:
	const std::vector<struct Quantile> quantiles;
	TYPE sum;
	size_t count;
	std::chrono::milliseconds max_age;
	int age_buckets;
	TimeWindowQuantiles<TYPE> quantile_values;
	std::vector<TYPE> quantile_out;
	std::vector<size_t> available_count;
};

////////// impl

template<typename TYPE>
GaugeVar<TYPE> *CounterVar<TYPE>::add(const LABEL_MAP& labels)
{
	std::string label_str;
	GaugeVar<TYPE> *var;

	if (!this->label_to_str(labels, label_str))
		return NULL;

	auto it = this->data.find(label_str);

	if (it == this->data.end())
	{
		var = new GaugeVar<TYPE>(label_str, "");
		this->data.insert(std::make_pair(label_str, var));
	}
	else
		var = it->second;

	return var;
}

template<typename TYPE>
bool CounterVar<TYPE>::reduce(const void *ptr, size_t)
{
	std::unordered_map<std::string, GaugeVar<TYPE> *> *data;
	data = (std::unordered_map<std::string, GaugeVar<TYPE> *> *)ptr;

	for (auto it = data->begin(); it != data->end(); it++)
	{
		auto my_it = this->data.find(it->first);

		if (my_it == this->data.end())
		{
			GaugeVar<TYPE> *var = new GaugeVar<TYPE>(it->first, "");
			this->data.insert(std::make_pair(it->first, var));
		}
		else
			my_it->second->reduce(it->second->get_data(),
								  it->second->get_size());
	}

	return true;
}

template<typename TYPE>
void CounterVar<TYPE>::collect(RPCVarCollector *collector)
{
	for (auto it = this->data.begin(); it != this->data.end(); it++)
		collector->collect_counter_each(this, it->first, it->second->get());
}

template<typename TYPE>
bool CounterVar<TYPE>::label_to_str(const LABEL_MAP& labels, std::string& str)
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

template<typename TYPE>
void HistogramVar<TYPE>::observe(const TYPE value)
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

template<typename TYPE>
bool HistogramVar<TYPE>::observe_multi(const std::vector<TYPE>& multi,
									   const TYPE sum)
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

template<typename TYPE>
bool HistogramVar<TYPE>::reduce(const void *ptr, size_t sz)
{
	if (sz != this->bucket_boundaries.size() + 1)
		return false;

	const HistogramVar<TYPE> *data = (const HistogramVar<TYPE> *)ptr;
	const std::vector<size_t> *src_bucket_counts = data->get_bucket_counts();

	for (size_t i = 0; i < sz; i++)
		this->bucket_counts[i] += (*src_bucket_counts)[i];

	this->bucket_counts[sz] += (*src_bucket_counts)[sz];
	this->sum += data->get_sum();
	this->count += data->get_count();

	return true;
}

template<typename TYPE>
void HistogramVar<TYPE>::collect(RPCVarCollector *collector)
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
	collector->collect_histogram_each(this, std::numeric_limits<TYPE>::max(),
									  current);

	collector->collect_histogram_end(this, this->sum, this->count);
}

template<typename TYPE>
void SummaryVar<TYPE>::observe(const TYPE value)
{
	this->quantile_values.insert(value);
	this->sum += value;
	this->count++;
}

template<typename TYPE>
bool SummaryVar<TYPE>::reduce(const void *ptr, size_t sz)
{
	if (sz != this->quantiles.size())
		return false;

	SummaryVar<TYPE> *data = (SummaryVar<TYPE> *)ptr;

	TimeWindowQuantiles<TYPE> *src = data->get_quantile_values();
	size_t available_count;
	TYPE get_val;

	for (size_t i = 0; i < sz; i ++)
	{
		available_count = src->get(this->quantiles[i].quantile, &get_val);
		this->quantile_out[i] += get_val * available_count;
		this->available_count[i] += available_count;
	}

	this->sum += data->get_sum();
	this->count += data->get_count();

	return true;
}

template<typename TYPE>
void SummaryVar<TYPE>::collect(RPCVarCollector *collector)
{
	collector->collect_summary_begin(this);

	for (size_t i = 0; i < this->quantiles.size(); i++)
	{
		collector->collect_summary_each(this, this->quantiles[i].quantile,
										this->quantile_out[i],
										this->available_count[i]);
	}

	collector->collect_summary_end(this, this->sum, this->count);
	this->quantile_out.clear();
	this->available_count.clear();
}

} // end namespace srpc

