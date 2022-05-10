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

#include "rpc_var_factory.h"

namespace srpc
{

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

} // end namespace srpc

