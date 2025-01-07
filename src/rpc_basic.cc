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

#include <workflow/json_parser.h>

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include "rpc_basic.h"
#include "rpc_module.h"

namespace srpc
{

void RPCCommon::log_format(std::string& key, std::string& value,
						   const RPCLogVector& fields)
{
	if (fields.size() == 0)
		return;

	char buffer[100];
	snprintf(buffer, 100, "%s%c%lld", SRPC_SPAN_LOG, ' ', GET_CURRENT_MS());
	key = std::move(buffer);
	value = "{\"";

	for (auto& field : fields)
	{
		value = value + std::move(field.first) + "\":\""
			  + std::move(field.second) + "\",";
	}

	value[value.length() - 1] = '}';
}

bool RPCCommon::addr_to_string(const struct sockaddr *addr,
							   char *ip_str, socklen_t len,
							   unsigned short *port)
{
	const char *ret = NULL;

	if (addr->sa_family == AF_INET)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;

		ret = inet_ntop(AF_INET, &sin->sin_addr, ip_str, len);
		*port = ntohs(sin->sin_port);
	}
	else if (addr->sa_family == AF_INET6)
	{
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

		ret = inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, len);
		*port = ntohs(sin6->sin6_port);
	}
	else
		errno = EINVAL;

	return ret != NULL;
}

bool RPCCommon::json_to_map(const std::string &json_str,
							std::map<std::string, std::string>& kv_map)
{
	json_value_t *val;
	const json_object_t *obj;
	const char *k;
	const json_value_t *v;
	bool ret = true;

	val = json_value_parse(json_str.c_str());
	if (val)
	{
		if (json_value_type(val) == JSON_VALUE_OBJECT)
		{
			obj = json_value_object(val);
			json_object_for_each(k, v, obj)
			{
				if (json_value_type(v) == JSON_VALUE_STRING)
				{
					kv_map.emplace(k, json_value_string(v));
				} else {
					ret = false;
					break;
				}
			}
		} else {
			ret = false;
		}

		json_value_destroy(val);
	}

	return ret;
}

} // namespace srpc

