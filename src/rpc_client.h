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

#ifndef __RPC_CLIENT_H__
#define __RPC_CLIENT_H__

#include "rpc_types.h"
#include "rpc_context.h"
#include "rpc_options.h"
#include "rpc_global.h"
#include "rpc_module_span.h"

namespace srpc
{

template<class RPCTYPE>
class RPCClient
{
public:
	using TASK = RPCClientTask<typename RPCTYPE::REQ, typename RPCTYPE::RESP>;

protected:
	using COMPLEXTASK = WFComplexClientTask<typename RPCTYPE::REQ,
											typename RPCTYPE::RESP>;

public:
	RPCClient(const std::string& service_name);
	virtual ~RPCClient() { };

	const RPCTaskParams *get_task_params() const;
	const std::string& get_service_name() const;

	void task_init(COMPLEXTASK *task) const;

	void set_keep_alive(int timeout);
	void set_watch_timeout(int timeout);
	void add_filter(RPCFilter *filter);

protected:
	template<class OUTPUT>
	TASK *create_rpc_client_task(const std::string& method_name,
								 std::function<void (OUTPUT *, RPCContext *)>&& done)
	{
		std::list<RPCModule *> module;
		for (int i = 0; i < SRPC_MODULE_MAX; i++)
		{
			if (this->modules[i])
				module.push_back(this->modules[i]);
		}

		auto *task = new TASK(this->service_name,
							  method_name,
							  &this->params.task_params,
							  std::move(module),
							  [done](int status_code, RPCWorker& worker) -> int {
				return ClientRPCDoneImpl(status_code, worker, done);
			});

		this->task_init(task);

		return task;
	}

	void init(const RPCClientParams *params);
	std::string service_name;

private:
	void __task_init(COMPLEXTASK *task) const;

protected:
	RPCClientParams params;
	ParsedURI uri;

private:
	struct sockaddr_storage ss;
	socklen_t ss_len;
	bool has_addr_info;
	std::mutex mutex;
	RPCModule *modules[SRPC_MODULE_MAX] = { 0 };
};

////////
// inl

template<class RPCTYPE>
inline RPCClient<RPCTYPE>::RPCClient(const std::string& service_name):
	params(RPC_CLIENT_PARAMS_DEFAULT),
	has_addr_info(false)
{
	SRPCGlobal::get_instance();
	this->service_name = service_name;
}

template<class RPCTYPE>
inline const RPCTaskParams *RPCClient<RPCTYPE>::get_task_params() const
{
	return &this->params.task_params;
}

template<class RPCTYPE>
inline const std::string& RPCClient<RPCTYPE>::get_service_name() const
{
	return this->service_name;
}

template<class RPCTYPE>
inline void RPCClient<RPCTYPE>::set_keep_alive(int timeout)
{
	this->params.task_params.keep_alive_timeout = timeout;
}

template<class RPCTYPE>
inline void RPCClient<RPCTYPE>::set_watch_timeout(int timeout)
{
	this->params.task_params.watch_timeout = timeout;
}

template<class RPCTYPE>
void RPCClient<RPCTYPE>::add_filter(RPCFilter *filter)
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
			case RPCModuleSpan:
				module = new RPCSpanModule<RPCTYPE>();
				break;
			case RPCModuleMonitor:
				module = new RPCMonitorModule<RPCTYPE>();
				break;
			case RPCModuleEmpty:
				module = new RPCEmptyModule<RPCTYPE>();
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

template<class RPCTYPE>
inline void RPCClient<RPCTYPE>::init(const RPCClientParams *params)
{
	this->params = *params;

	if (this->params.task_params.data_type == RPCDataUndefined)
		this->params.task_params.data_type = RPCTYPE::default_data_type;

	this->has_addr_info = SRPCGlobal::get_instance()->task_init(this->params,
																this->uri,
																&this->ss,
																&this->ss_len);
}

template<class RPCTYPE>
inline void RPCClient<RPCTYPE>::__task_init(COMPLEXTASK *task) const
{
	if (this->has_addr_info)
		task->init(this->params.is_ssl ? TT_TCP_SSL : TT_TCP,
				   (const struct sockaddr *)&this->ss, this->ss_len, "");
	else
	{
		task->init(this->uri);
		task->set_transport_type(this->params.is_ssl ? TT_TCP_SSL : TT_TCP);
	}
}

template<class RPCTYPE>
inline void RPCClient<RPCTYPE>::task_init(COMPLEXTASK *task) const
{
	__task_init(task);
}

static inline void __set_host_by_uri(const ParsedURI *uri, bool is_ssl,
									 std::string& header_host)
{
	if (uri->host && uri->host[0])
		header_host = uri->host;

	if (uri->port && uri->port[0])
	{
		int port = atoi(uri->port);

		if (is_ssl)
		{
			if (port != 443)
			{
				header_host += ":";
				header_host += uri->port;
			}
		}
		else
		{
			if (port != 80)
			{
				header_host += ":";
				header_host += uri->port;
			}
		}
	}
}

template<>
inline void RPCClient<RPCTYPESRPCHttp>::task_init(COMPLEXTASK *task) const
{
	__task_init(task);
	std::string header_host;

	if (this->has_addr_info)
		header_host += this->params.host + ":" + std::to_string(this->params.port);
	else
		__set_host_by_uri(task->get_current_uri(), this->params.is_ssl, header_host);

	task->get_req()->set_header_pair("Host", header_host.c_str());
}

template<>
inline void RPCClient<RPCTYPEThriftHttp>::task_init(COMPLEXTASK *task) const
{
	__task_init(task);
	std::string header_host;

	if (this->has_addr_info)
		header_host += this->params.host + ":" + std::to_string(this->params.port);
	else
		__set_host_by_uri(task->get_current_uri(), this->params.is_ssl, header_host);

	task->get_req()->set_header_pair("Host", header_host.c_str());
}

} // namespace srpc

#endif

