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

#ifndef __RPC_VAR_H__
#define __RPC_VAR_H__

#include <utility>
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include "time_window_quantiles.h"

namespace srpc
{

class RPCVar;
class RPCVarLocal;
class GaugeVar;
class CounterVar;
class HistogramVar;
class SummaryVar;
class HistogramCounterVar;

enum RPCVarType
{
	VAR_GAUGE				=	0,
	VAR_COUNTER				=	1,
	VAR_HISTOGRAM			=	2,
	VAR_SUMMARY				=	3,
	VAR_HISTOGRAM_COUNTER	=	4
};

static std::string type_string(RPCVarType type)
{
	switch (type)
	{
	case VAR_GAUGE:
		return "gauge";
	case VAR_COUNTER:
		return "counter";
	case VAR_HISTOGRAM:
	case VAR_HISTOGRAM_COUNTER:
		return "histogram";
	case VAR_SUMMARY:
		return "summary";
	default:
		break;
	}
	return "";
}

class RPCVarFactory
{
public:
	// thread local api
	static GaugeVar *gauge(const std::string& name);

	static CounterVar *counter(const std::string& name);

	static HistogramVar *histogram(const std::string& name);

	static SummaryVar *summary(const std::string& name);

	static HistogramCounterVar *histogram_counter(const std::string &name);

	static RPCVar *var(const std::string& name);
	static bool check_name_format(const std::string& name);
};

class RPCVarGlobal
{
public:
	static RPCVarGlobal *get_instance()
	{
		static RPCVarGlobal kInstance;
		return &kInstance;
	}

	void add(RPCVarLocal *local)
	{
		this->mutex.lock();
		this->local_vars.push_back(local);
		this->mutex.unlock();
	}

	RPCVar *find(const std::string& name);

	// following APIs are used when thread exit and its ~RPCVarLocal()

	// remove a RPCVarLocal from Glolba->local_vars
	void remove(const RPCVarLocal *var);
	// duplicate the vars into global existing RPCVarLocal
	void dup(const std::unordered_map<std::string, RPCVar *>& vars);

private:
	RPCVarGlobal() { this->finished = false; }
	~RPCVarGlobal();

public:
	std::mutex mutex;
	std::vector<RPCVarLocal *> local_vars;
	bool finished;
};

class RPCVarLocal
{
public:
	static RPCVarLocal *get_instance()
	{
		static thread_local RPCVarLocal kInstance;
		return &kInstance;
	}

	void add(std::string name, RPCVar *var)
	{
		this->mutex.lock();
		const auto it = this->vars.find(name);

		if (it == this->vars.end())
			this->vars.insert(std::make_pair(std::move(name), var));

		this->mutex.unlock();
	}

	virtual ~RPCVarLocal();

private:
	RPCVarLocal()
	{
		RPCVarGlobal::get_instance()->add(this);
	}

public:
	std::mutex mutex;
	std::unordered_map<std::string, RPCVar *> vars;
	friend class RPCVarGlobal;
};

class RPCVarCollector
{
public:
	virtual void collect_gauge(RPCVar *gauge, double data) = 0;

	virtual void collect_counter_each(RPCVar *counter, const std::string& label,
									  double data) = 0;

	virtual void collect_histogram_begin(RPCVar *histogram) = 0;
	virtual void collect_histogram_each(RPCVar *histogram,
										double bucket_boudary,
										size_t current_count) = 0;
	virtual void collect_histogram_end(RPCVar *histogram, double sum,
									   size_t count) = 0;

	virtual void collect_summary_begin(RPCVar *summary) = 0;
	virtual void collect_summary_each(RPCVar *summary, double quantile,
									  double quantile_out) = 0;
	virtual void collect_summary_end(RPCVar *summary, double sum,
									 size_t count) = 0;
};

class RPCVar
{
public:
	const std::string& get_name() const { return this->name; }
	const std::string& get_help() const { return this->help; }
	RPCVarType get_type() const { return this->type; }
	const std::string get_type_str() const { return type_string(this->type); }

	virtual RPCVar *create(bool with_data) = 0;
	virtual bool reduce(const void *ptr, size_t sz) = 0;
	virtual size_t get_size() const = 0;
	virtual const void *get_data() = 0;
	virtual void collect(RPCVarCollector *collector) = 0;
	virtual void reset() = 0;

public:
	RPCVar(const std::string& name, const std::string& help, RPCVarType type) :
		name(name),
		help(help),
		type(type)
	{
//		this->format_name();
	}

	virtual ~RPCVar() {}

private:
	void format_name();

protected:
	std::string name;
	std::string help;
	RPCVarType type;
};

class GaugeVar : public RPCVar
{
public:
	virtual void increase() { ++this->data; }
	virtual void decrease() { --this->data; }
	size_t get_size() const override { return sizeof(double); }
	const void *get_data() override { return &this->data; }

	virtual double get() { return this->data; }
	virtual void set(double var) { this->data = var; }

	RPCVar *create(bool with_data) override;

	bool reduce(const void *ptr, size_t sz) override
	{
		this->data += *(double *)ptr;
//		fprintf(stderr, "reduce data=%d *ptr=%d\n", this->data, *(double *)ptr);
		return true;
	}

	void collect(RPCVarCollector *collector) override
	{
		collector->collect_gauge(this, this->data);
	}

	void reset() override { this->data = 0; }

public:
	GaugeVar(const std::string& name, const std::string& help) :
		RPCVar(name, help, VAR_GAUGE)
	{
		this->data = 0;
	}

private:
	double data;
};

class CounterVar : public RPCVar
{
public:
	using LABEL_MAP = std::map<std::string, std::string>;
	GaugeVar *add(const LABEL_MAP& labels);
	void increase(const LABEL_MAP& labels);
	void increase(const LABEL_MAP &labels, double value);

	RPCVar *create(bool with_data) override;
	bool reduce(const void *ptr, size_t sz) override;
	void collect(RPCVarCollector *collector) override;

	size_t get_size() const override { return this->data.size(); }
	const void *get_data() override { return this; }
	const std::unordered_map<std::string, GaugeVar *> *get_map() const
	{
		return &this->data;
	}
	double get_sum() const { return this->sum; }
	void increase_sum(double value) { this->sum += value; }

	void reset() override;

public:
	CounterVar(const std::string &name, const std::string &help);
	virtual ~CounterVar();

private:
	std::unordered_map<std::string, GaugeVar *> data;
	double sum;
};

class HistogramVar : public RPCVar
{
public:
	void observe(double value);

	// multi is the histogram count of each bucket, including +inf
	bool observe_multi(const std::vector<size_t>& multi, double sum);

	RPCVar *create(bool with_data) override;
	bool reduce(const void *ptr, size_t sz) override;
	void collect(RPCVarCollector *collector) override;

	size_t get_size() const override { return this->bucket_counts.size(); }
	const void *get_data() override { return this; }

	double get_sum() const { return this->sum; }
	size_t get_count() const { return this->count; }
	const std::vector<double> *get_bucket_boundaries() const
	{
		return &this->bucket_boundaries;
	}
	const std::vector<size_t> *get_bucket_counts() const
	{
		return &this->bucket_counts;
	}

	void reset() override;

public:
	HistogramVar(const std::string& name, const std::string& help,
				 const std::vector<double>& bucket);

private:
	std::vector<double> bucket_boundaries;
	std::vector<size_t> bucket_counts;
	double sum;
	size_t count;
};

class SummaryVar : public RPCVar
{
public:
	void observe(double value);

	RPCVar *create(bool with_data) override;
	bool reduce(const void *ptr, size_t sz) override;
	void collect(RPCVarCollector *collector) override;

	size_t get_size() const override { return this->quantiles.size(); }
	const void *get_data() override { return this; }

	double get_sum() const { return this->sum; }
	size_t get_count() const { return this->count; }
	TimeWindowQuantiles<double> *get_quantile_values()
	{
		return &this->quantile_values;
	}

	void reset() override { /* TODO: incorrect if reset() by report interval */ }

	const std::vector<struct Quantile>& get_quantiles() const
	{
		return this->quantiles;
	}
	const std::vector<double>& get_quantile_out() const
	{
		return this->quantile_out;
	}

	// only for clear stack variable after filled into protobuf or out_string
	void clear_quantile_out() { this->quantile_out.clear(); }

public:
	SummaryVar(const std::string& name, const std::string& help,
			   const std::vector<struct Quantile>& quantile,
			   const std::chrono::milliseconds max_age, int age_bucket);

private:
	std::vector<struct Quantile> quantiles;
	double sum;
	size_t count;
	size_t quantile_size;
	std::chrono::milliseconds max_age;
	int age_buckets;
	TimeWindowQuantiles<double> quantile_values;
	std::vector<double> quantile_out;
};

template<typename VAR>
class RPCTimeWindow
{
public:
	using Clock = std::chrono::steady_clock;
	RPCTimeWindow(Clock::duration duration, size_t bucket_num);

protected:
	VAR& rotate();

protected:
	std::vector<VAR> data_bucket;
	Clock::duration duration;
	size_t bucket_num;

private:
	size_t current_bucket;
	Clock::time_point last_rotation;
	Clock::duration rotation_interval;
};

template<typename VAR>
RPCTimeWindow<VAR>::RPCTimeWindow(Clock::duration duration, size_t bucket_num) :
	duration(duration),
	bucket_num(bucket_num),
	rotation_interval(duration / bucket_num)
{
	this->current_bucket = 0;
	this->last_rotation = Clock::now();
}

template<typename VAR>
VAR& RPCTimeWindow<VAR>::rotate()
{
	auto delta = Clock::now() - this->last_rotation;

	while (delta > this->rotation_interval)
	{
		this->data_bucket[this->current_bucket].reset();
		if (++this->current_bucket >= this->data_bucket.size())
			this->current_bucket = 0;

		delta -= this->rotation_interval;
		this->last_rotation += this->rotation_interval;
	}

	return this->data_bucket[this->current_bucket];
}

class TimedGaugeVar : public GaugeVar, RPCTimeWindow<GaugeVar>
{
public:
	TimedGaugeVar(const std::string& name, const std::string& help,
				  Clock::duration duration, size_t bucket_num);
	// for collect
	void increase() override;
	double get() override;
	void set(double var) override;

	// for reduce
	const void *get_data() override;
	RPCVar *create(bool with_data) override;
};

class HistogramCounterVar : public RPCVar
{
public:
	using LABEL_MAP = std::map<std::string, std::string>;
	void observe(const LABEL_MAP &labels, double value);

	RPCVar *create(bool with_data) override;
	bool reduce(const void *ptr, size_t sz) override;
	void collect(RPCVarCollector *collector) override;

	size_t get_size() const override { return this->data.size(); }
	const void *get_data() override { return this; }
	const std::unordered_map<std::string, HistogramVar *> *get_map() const
	{
		return &this->data;
	}

	void reset() override;

public:
	HistogramCounterVar(const std::string &name, const std::string &help,
						const std::vector<double> &bucket);
	virtual ~HistogramCounterVar();

private:
	std::unordered_map<std::string, HistogramVar *> data;
	std::vector<double> bucket;
};

} // end namespace srpc

#endif

