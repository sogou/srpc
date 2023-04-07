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

#include "http_client.h"
#include "http_task.h"
#include "http_module.h"

namespace srpc
{

HttpClient::HttpClient(const std::string& url) : 
	params(HTTP_CLIENT_PARAMS_DEFAULT)
{
//	TODO: make sure this is necessary
	this->params.url = url;
	this->init();
}

HttpClient::HttpClient(const ParsedURI& uri) :
	params(HTTP_CLIENT_PARAMS_DEFAULT)
{
	this->params.uri = uri;
	if (this->params.uri.scheme &&
		strcasecmp(this->params.uri.scheme, "https") == 0)
	{
		this->params.is_ssl = true;
//		this->params.port = HTTP_SSL_PORT_DEFAULT;
	}
}

HttpClient::HttpClient(const HttpClientParams *params) :
	params(*params)
{
	this->init();
}

WFHttpTask *HttpClient::create_http_task(http_callback_t callback)
{
	std::list<RPCModule *> module;
	for (int i = 0; i < SRPC_MODULE_MAX; i++)
	{
		if (this->modules[i])
			module.push_back(this->modules[i]);
	}

	auto&& cb = std::bind(&HttpClient::callback, this, std::placeholders::_1);

	HttpClientTask *task = new HttpClientTask(this->params.task_params.redirect_max,
											  this->params.task_params.retry_max,
											  cb, std::move(module));

	task->user_callback = std::move(callback);
	this->task_init(task);

	return task;
}

void HttpClient::init()
{
	URIParser::parse(this->params.url, this->params.uri);

	if (this->params.uri.scheme &&
		strcasecmp(this->params.uri.scheme, "https") == 0)
	{
		this->params.is_ssl = true;
//		this->params.port = HTTP_SSL_PORT_DEFAULT;
	}
}

void HttpClient::task_init(HttpClientTask *task)
{
	// set task by this->params;
	task->init(this->params.uri);
	task->set_transport_type(this->params.is_ssl ? TT_TCP_SSL : TT_TCP);
	task->set_send_timeout(this->params.task_params.send_timeout);
	task->set_receive_timeout(this->params.task_params.receive_timeout);
	task->set_keep_alive(this->params.task_params.keep_alive_timeout);
}

void HttpClient::callback(WFHttpTask *task)
{
	RPCModuleData resp_data;
	http_get_header_module_data(task->get_resp(), resp_data);
	
	for (int i = 0; i < SRPC_MODULE_MAX; i++)
	{
		if (this->modules[i])
			this->modules[i]->client_task_end(task, resp_data);
	}

	HttpClientTask *client_task = (HttpClientTask *)task;
	if (client_task->user_callback)
		client_task->user_callback(task);
}

void HttpClient::add_filter(RPCFilter *filter)
{
	int type = filter->get_module_type();

	this->mutex.lock();
	if (type < SRPC_MODULE_MAX && type >= 0)
	{
		RPCModule *module = this->modules[type];

		if (!module)
		{
			switch (type)
			{
			case RPCModuleTypeTrace:
				module = new HttpTraceModule();
				break;
			case RPCModuleTypeMetrics:
				module = new HttpMetricsModule();
				break;
			default:
				break;
			}
			this->modules[type] = module;
		}

		if (module)
			module->add_filter(filter);
	}

	this->mutex.unlock();
	return;
}

} // end namespace srpc

