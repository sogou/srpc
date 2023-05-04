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

#ifndef __RPC_HTTP_CLIENT_H__
#define __RPC_HTTP_CLIENT_H__

#include "workflow/WFTask.h"
#include "workflow/HttpMessage.h"
#include "workflow/WFTaskFactory.h"
#include "rpc_options.h"
#include "rpc_basic.h"
#include "http_task.h"

namespace srpc
{

#define HTTP_REDIRECT_DEFAULT	2
#define HTTP_RETRY_DEFAULT		5
#define HTTP_KEEPALIVE_DEFAULT	(60 * 1000)

struct HttpTaskParams
{
	int send_timeout;
	int receive_timeout;
	int watch_timeout;
	int keep_alive_timeout;
	int redirect_max;
	int retry_max;
};

struct HttpClientParams
{
	HttpTaskParams task_params;
	bool is_ssl;
	std::string url; // can be empty
	ParsedURI uri;
};

static constexpr struct HttpTaskParams HTTP_TASK_PARAMS_DEFAULT =
{
/*	.send_timeout		=	*/	-1,
/*	.receive_timeout	=	*/	-1,
/*	.watch_timeout		=	*/	0,
/*	.keep_alive_timeout	=	*/	HTTP_KEEPALIVE_DEFAULT,
/*	.retry_max			=	*/	HTTP_REDIRECT_DEFAULT,
/*	.redirect_max		=	*/	HTTP_RETRY_DEFAULT,
};

static const struct HttpClientParams HTTP_CLIENT_PARAMS_DEFAULT = 
{
/*	.task_params		=	*/	HTTP_TASK_PARAMS_DEFAULT,
/*	.is_ssl				=	*/	false,
/*	.url				=	*/	"",
/*	.uri				=	*/
};

class HttpClient
{
public:
	HttpClient() { }

	WFHttpTask *create_http_task(const std::string& url,
								 int redirect_max,
								 int retry_max,
								 http_callback_t callback);

	WFHttpTask *create_http_task(const ParsedURI& uri,
								 int redirect_max,
								 int retry_max,
								 http_callback_t callback);

	void add_filter(RPCFilter *filter);

private:
	void callback(WFHttpTask *task);
	void init();
	void task_init(HttpClientTask *task);

protected:
	HttpClientParams params;
	ParsedURI uri;

private:
	std::mutex mutex;
	RPCModule *modules[SRPC_MODULE_MAX] = { 0 };
};

} // end namespace srpc

#endif

