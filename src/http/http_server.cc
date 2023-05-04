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

#include "http_server.h"
#include "http_task.h"
#include "http_module.h"

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace srpc
{

void HttpServer::add_filter(RPCFilter *filter)
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

CommSession *HttpServer::new_session(long long seq, CommConnection *conn)
{
	HttpServerTask *task;

	std::list<RPCModule *> module;
	for (int i = 0; i < SRPC_MODULE_MAX; i++)
	{
		if (this->modules[i])
			module.push_back(this->modules[i]);
	}

	task = new HttpServerTask(this, std::move(module), this->process);
	task->set_keep_alive(this->params.keep_alive_timeout);
	task->set_receive_timeout(this->params.receive_timeout);
	task->get_req()->set_size_limit(this->params.request_size_limit);
	task->set_is_ssl(this->get_ssl_ctx() != NULL);
	task->set_listen_port(this->get_listen_port());

	return task;
}

unsigned short HttpServer::get_listen_port()
{
	if (this->listen_port == 0)
	{
		struct sockaddr_storage addr;
		socklen_t l = sizeof addr;
		this->get_listen_addr((struct sockaddr *)&addr, &l);
		if (addr.ss_family == AF_INET)
		{
			struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
			this->listen_port = ntohs(sin->sin_port);
		}
		else // if (addr.ss_family == AF_INET6)
		{
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
			this->listen_port = ntohs(sin6->sin6_port);
		}
	}

	return this->listen_port;
}

} // namespace srpc

