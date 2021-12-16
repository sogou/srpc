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

#ifndef __RPC_OPTIONS_H__
#define __RPC_OPTIONS_H__

#include <list>
#include <string>
#include <workflow/WFServer.h>
#include <workflow/URIParser.h>
#include "rpc_basic.h"

namespace srpc {

struct RPCTaskParams
{
	int send_timeout;
	int receive_timeout;
	int watch_timeout;
	int keep_alive_timeout;
	int retry_max;
	int compress_type;	//RPCCompressType
	int data_type;		//RPCDataType
};

struct RPCClientParams
{
	RPCTaskParams task_params;
//host + port + is_ssl
	std::string host;
	unsigned short port;
	bool is_ssl;
//or URL
	std::string url;
	std::string caller;
};

struct RPCServerParams : public WFServerParams
{
	RPCServerParams() :
		WFServerParams({
/*	.max_connections		=	*/	2000,
/*	.peer_response_timeout	=	*/	10 * 1000,
/*	.receive_timeout		=	*/	-1,
/*	.keep_alive_timeout		=	*/	60 * 1000,
/*	.request_size_limit		=	*/	RPC_BODY_SIZE_LIMIT,
/*	.ssl_accept_timeout		=	*/	10 * 1000
		})
	{}
};

static constexpr struct RPCTaskParams RPC_TASK_PARAMS_DEFAULT =
{
/*	.send_timeout		=	*/	-1,
/*	.receive_timeout	=	*/	-1,
/*	.watch_timeout		=	*/	0,
/*	.keep_alive_timeout	=	*/	30 * 1000,
/*	.retry_max			=	*/	0,
/*	.compress_type		=	*/	RPCCompressNone,
/*	.data_type			=	*/	RPCDataUndefined
};

static const struct RPCClientParams RPC_CLIENT_PARAMS_DEFAULT =
{
/*	.task_params		=	*/	RPC_TASK_PARAMS_DEFAULT,
/*	.host				=	*/	"",
/*	.port				=	*/	SRPC_DEFAULT_PORT,
/*	.is_ssl				=	*/	false,
/*	.url				=	*/	"",
/*	.caller				=	*/	""
};

static const RPCServerParams RPC_SERVER_PARAMS_DEFAULT;

} // end namespace

#endif

