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

} // namespace srpc

