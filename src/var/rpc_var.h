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

#ifndef __RPC_VAR_H__
#define __RPC_VAR_H__

#include <utility>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

namespace srpc
{

class RPCVar;
class RPCVarLocal;

enum RPCVarType
{
	VAR_GAUGE		=	0,
	VAR_COUNTER		=	1,
	VAR_HISTOGRAM	=	2,
	VAR_SUMMARY		=	3
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
		return "histogram";
	case VAR_SUMMARY:
		return "summary";
	default:
		break;
	}
	return "";
}

class RPCVarGlobal
{
public:
	static RPCVarGlobal *get_instance()
	{
		static RPCVarGlobal kInstance;
		return &kInstance;
	}

	void add(RPCVarLocal *var)
	{
		this->mutex.lock();
		this->local_vars.push_back(var);
		this->mutex.unlock();
	}

	void del(const RPCVarLocal *var);
	RPCVar *find(const std::string& name);
	void dup(const std::unordered_map<std::string, RPCVar *>& vars);

private:
	RPCVarGlobal() { }

public:
	std::mutex mutex;
	std::vector<RPCVarLocal *> local_vars;
//	friend class RPCVarFactory;
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
//	friend class RPCVarFactory;
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
									  double quantile_out,
									  size_t available_count) = 0;
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
	virtual const void *get_data() const = 0;
	virtual void collect(RPCVarCollector *collector) = 0;

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

} // end namespace srpc

#include "rpc_var.inl"

#endif

