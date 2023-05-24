/*
  Copyright (c) 2023 Sogou, Inc.

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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <gtest/gtest.h>
#include "srpc/rpc_var.h"

using namespace srpc;

RPCVar *get_and_reduce(std::string&& var_name)
{
	RPCVar *result = NULL;
	RPCVarGlobal *global_var = RPCVarGlobal::get_instance();
	std::unordered_map<std::string, RPCVar *>::iterator it;

	global_var->mutex.lock();
	for (RPCVarLocal *local : global_var->local_vars)
	{
		local->mutex.lock();
		it = local->vars.find(var_name);
		if (it != local->vars.end())
		{
			if (result == NULL)
				result = it->second->create(true);
			else
				result->reduce(it->second->get_data(), it->second->get_size());
		}
		local->mutex.unlock();
	}
	global_var->mutex.unlock();

	return result;
}

TEST(var_unittest, GaugeVar)
{
	GaugeVar *req = new GaugeVar("req", "total req");
	RPCVarLocal::get_instance()->add("req", req);

	RPCVarFactory::gauge("req")->increase();

	GaugeVar *gauge = (GaugeVar *)get_and_reduce("req");
	EXPECT_EQ(gauge->get(), 1.0);
	delete gauge;
}

TEST(var_unittest, TimedGaugeVar)
{
	TimedGaugeVar *qps = new TimedGaugeVar("qps", "req per second",
										   std::chrono::seconds(1), 4);
	RPCVarLocal::get_instance()->add("qps", qps);

	RPCVarFactory::gauge("qps")->increase();
	RPCVarFactory::gauge("qps")->increase();

	GaugeVar *gauge = (GaugeVar *)get_and_reduce("qps");
	EXPECT_EQ(gauge->get(), 2.0);
	delete gauge;

	usleep(500000);
	RPCVarFactory::gauge("qps")->increase();
	gauge = (GaugeVar *)get_and_reduce("qps");
	EXPECT_EQ(gauge->get(), 3.0);
	delete gauge;

	usleep(500000);
	gauge = (GaugeVar *)get_and_reduce("qps");
	EXPECT_EQ(gauge->get(), 1.0);
	delete gauge;
}

