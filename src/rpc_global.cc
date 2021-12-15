/*
  Copyright (c) 2020 Sogou, Inc.

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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <workflow/WFGlobal.h>
#include <workflow/URIParser.h>
#include "rpc_basic.h"
#include "rpc_global.h"

namespace srpc
{

SRPCGlobal::SRPCGlobal() :
	gen(rd())
{
	WFGlobal::register_scheme_port(SRPC_SCHEME, SRPC_DEFAULT_PORT);
	WFGlobal::register_scheme_port(SRPC_SSL_SCHEME, SRPC_SSL_DEFAULT_PORT);
	this->group_id = this->gen() % (1 << SRPC_GROUP_BITS);
	this->machine_id = this->gen() % (1 << SRPC_MACHINE_BITS);
}

static int __get_addr_info(const std::string& host, unsigned short port,
						   struct sockaddr_storage *ss, socklen_t *ss_len)
{
	if (!host.empty())
	{
		char front = host.front();
		char back = host.back();

		if (host.find(':') != std::string::npos)
		{
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;

			memset(sin6, 0, sizeof (struct sockaddr_in6));
			if (inet_pton(AF_INET6, host.c_str(), &sin6->sin6_addr) == 1)
			{
				sin6->sin6_family = AF_INET6;
				sin6->sin6_port = htons(port);
				*ss_len = sizeof (struct sockaddr_in6);
				return 0;
			}
		}
		else if (isdigit(back) && isdigit(front))
		{
			struct sockaddr_in *sin = (struct sockaddr_in *)ss;

			memset(sin, 0, sizeof (struct sockaddr_in));
			if (inet_pton(AF_INET, host.c_str(), &sin->sin_addr) == 1)
			{
				sin->sin_family = AF_INET;
				sin->sin_port = htons(port);
				*ss_len = sizeof (struct sockaddr_in);
				return 0;
			}
		}
	}

	return -1;
}

bool SRPCGlobal::task_init(RPCClientParams& params, ParsedURI& uri,
						   struct sockaddr_storage *ss, socklen_t *ss_len) const
{
	if (!params.host.empty())
	{
		if (__get_addr_info(params.host, params.port, ss, ss_len) == 0)
			return true;

		if (params.is_ssl)
			uri.scheme = strdup(SRPC_SSL_SCHEME);
		else
			uri.scheme = strdup(SRPC_SCHEME);

		uri.host = strdup(params.host.c_str());
		uri.port = strdup(std::to_string(params.port).c_str());
		if (uri.scheme && uri.host && uri.port)
			uri.state = URI_STATE_SUCCESS;
		else
		{
			uri.state = URI_STATE_ERROR;
			uri.error = errno;
		}
	}
	else
	{
		URIParser::parse(params.url, uri);
		params.is_ssl = (uri.scheme &&
								(strcasecmp(uri.scheme, "https") == 0 ||
								 strcasecmp(uri.scheme, "srpcs") == 0));
	}

	return false;
}

unsigned long long SRPCGlobal::get_random()
{
	unsigned long long id = 0;
	this->snowflake.get_id(this->group_id, this->machine_id, &id);
	return id;
}

static const SRPCGlobal *srpc_global = SRPCGlobal::get_instance();

} // namespace srpc

