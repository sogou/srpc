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

#ifndef __RPC_VAR_FACTORY_H__
#define __RPC_VAR_FACTORY_H__

#include "rpc_var.h"

namespace srpc
{

class RPCVarFactory
{
public:
	// thread local api
	template<typename TYPE>
	static GaugeVar<TYPE> *gauge(const std::string& name);

	template<typename TYPE>
	static CounterVar<TYPE> *counter(const std::string& name);

	template<typename TYPE>
	static HistogramVar<TYPE> *histogram(const std::string& name);

	template<typename TYPE>
	static SummaryVar<TYPE> *summary(const std::string& name);

	static RPCVar *var(const std::string& name);
	static bool check_name_format(const std::string& name);
};

template<typename TYPE>
GaugeVar<TYPE> *RPCVarFactory::gauge(const std::string& name)
{
	return static_cast<GaugeVar<TYPE> *>(RPCVarFactory::var(name));
}

template<typename TYPE>
CounterVar<TYPE> *RPCVarFactory::counter(const std::string& name)
{
	return static_cast<CounterVar<TYPE> *>(RPCVarFactory::var(name));
}

template<typename TYPE>
HistogramVar<TYPE> *RPCVarFactory::histogram(const std::string& name)
{
	return static_cast<HistogramVar<TYPE> *>(RPCVarFactory::var(name));
}

template<typename TYPE>
SummaryVar<TYPE> *RPCVarFactory::summary(const std::string& name)
{
	return static_cast<SummaryVar<TYPE> *>(RPCVarFactory::var(name));
}

} // end namespace srpc

#endif

