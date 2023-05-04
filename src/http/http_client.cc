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

WFHttpTask *HttpClient::create_http_task(const std::string& url,
										 int redirect_max,
										 int retry_max,
										 http_callback_t callback)
{
	std::list<RPCModule *> module;
	for (int i = 0; i < SRPC_MODULE_MAX; i++)
	{
		if (this->modules[i])
			module.push_back(this->modules[i]);
	}

	auto&& cb = std::bind(&HttpClient::callback, this, std::placeholders::_1);

	HttpClientTask *task = new HttpClientTask(redirect_max, retry_max,
											  std::move(module), std::move(cb));

	ParsedURI uri;
	URIParser::parse(url, uri);
	task->init(std::move(uri));
	task->set_keep_alive(HTTP_KEEPALIVE_DEFAULT);
//	task->set_url(url);
	task->user_callback_ = std::move(callback);

	return task;
}

WFHttpTask *HttpClient::create_http_task(const ParsedURI& uri,
										 int redirect_max,
										 int retry_max,
										 http_callback_t callback)
{
	std::list<RPCModule *> module;
	for (int i = 0; i < SRPC_MODULE_MAX; i++)
	{
		if (this->modules[i])
			module.push_back(this->modules[i]);
	}

	auto&& cb = std::bind(&HttpClient::callback, this, std::placeholders::_1);

	HttpClientTask *task = new HttpClientTask(redirect_max, retry_max,
											  std::move(module), std::move(cb));

	task->init(uri);
	task->set_keep_alive(HTTP_KEEPALIVE_DEFAULT);
//	task->set_url(url);
	task->user_callback_ = std::move(callback);

	return task;
}

void HttpClient::init()
{
	URIParser::parse(this->params.url, this->params.uri);

	if (this->params.uri.scheme &&
		strcasecmp(this->params.uri.scheme, "https") == 0)
	{
		this->params.is_ssl = true;
	}
}

void HttpClient::callback(WFHttpTask *task)
{
	HttpClientTask *client_task = (HttpClientTask *)task;
	RPCModuleData *resp_data;
	HttpServerTask::ModuleSeries *series;
	const std::list<RPCModule *>& module_list = client_task->get_module_list();

	if (!module_list.empty())
	{
		resp_data = client_task->mutable_module_data();

		if (resp_data->empty()) // get series module data failed previously
		{
			series = dynamic_cast<HttpServerTask::ModuleSeries *>(series_of(task));

			if (series)
				resp_data = series->get_module_data();
		}
//		else
//			http_get_header_module_data(task->get_resp(), *resp_data);

		for (RPCModule *module : module_list)
			module->client_task_end(task, *resp_data);
	}

	if (client_task->user_callback_)
		client_task->user_callback_(task);
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

