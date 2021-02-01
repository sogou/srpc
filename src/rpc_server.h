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

#ifndef __RPC_SERVER_H__
#define __RPC_SERVER_H__

#include <string>
#include <map>
#include <errno.h>
#include <workflow/WFServer.h>
#include <workflow/WFHttpServer.h>
#include "rpc_types.h"
#include "rpc_service.h"
#include "rpc_options.h"

namespace srpc
{

template<class RPCTYPE>
class RPCServer : public WFServer<typename RPCTYPE::REQ, typename RPCTYPE::RESP>
{
public:
	using REQTYPE = typename RPCTYPE::REQ;
	using RESPTYPE = typename RPCTYPE::RESP;
	using TASK = RPCServerTask<REQTYPE, RESPTYPE>;

protected:
	using NETWORKTASK = WFNetworkTask<REQTYPE, RESPTYPE>;

public:
	RPCServer();
	RPCServer(const struct RPCServerParams *params);


	int add_service(RPCService *service);
	const RPCService* find_service(const std::string& name) const;

protected:
	RPCServer(const struct RPCServerParams *params,
			  std::function<void (NETWORKTASK *)>&& process);

	CommSession *new_session(long long seq, CommConnection *conn) override;
	void server_process(NETWORKTASK *task) const;

private:
	void set_tracing(TASK *Task);

	std::map<std::string, RPCService *> service_map;
	RPCSpanLogger *span_logger;
};

////////
// inl

template<class RPCTYPE>
inline RPCServer<RPCTYPE>::RPCServer():
	WFServer<REQTYPE, RESPTYPE>(&RPC_SERVER_PARAMS_DEFAULT,
								std::bind(&RPCServer::server_process,
								this, std::placeholders::_1)),
	span_logger(NULL)
{}

template<class RPCTYPE>
inline RPCServer<RPCTYPE>::RPCServer(const struct RPCServerParams *params):
	WFServer<REQTYPE, RESPTYPE>(params,
								std::bind(&RPCServer::server_process,
								this, std::placeholders::_1)),
	span_logger(params->span_logger)
{}

template<class RPCTYPE>
inline RPCServer<RPCTYPE>::RPCServer(const struct RPCServerParams *params,
									 std::function<void (NETWORKTASK *)>&& process):
	WFServer<REQTYPE, RESPTYPE>(&params, std::move(process)),
	span_logger(params->span_logger)
{}

template<class RPCTYPE>
inline int RPCServer<RPCTYPE>::add_service(RPCService* service)
{
	const auto it = this->service_map.emplace(service->get_name(), service);

	if (!it.second)
	{
		errno = EEXIST;
		return -1;
	}

	return 0;
}

template<class RPCTYPE>
inline const RPCService *RPCServer<RPCTYPE>::find_service(const std::string& name) const
{
	const auto it = this->service_map.find(name);

	if (it != this->service_map.cend())
		return it->second;

	return NULL;
}

template<class RPCTYPE>
inline CommSession *RPCServer<RPCTYPE>::new_session(long long seq,
													CommConnection *conn)
{
	/* TODO: Change to a factory function. */
	auto *task = new TASK(this, this->process, this->span_logger);

	task->set_keep_alive(this->params.keep_alive_timeout);
	task->get_req()->set_size_limit(this->params.request_size_limit);

	return task;
}

template<class RPCTYPE>
void RPCServer<RPCTYPE>::server_process(NETWORKTASK *task) const
{
	auto *req = task->get_req();
	auto *resp = task->get_resp();
	int status_code;

	if (!req->deserialize_meta())
		status_code = RPCStatusMetaError;
	else
	{
		RPCTYPE::server_reply_init(req, resp);
		auto *service = this->find_service(req->get_service_name());

		if (!service)
			status_code = RPCStatusServiceNotFound;
		else
		{
			auto *rpc = service->find_method(req->get_method_name());

			if (!rpc)
				status_code = RPCStatusMethodNotFound;
			else
			{
				if (this->span_logger)
					static_cast<TASK *>(task)->start_span();

				status_code = req->decompress();
				if (status_code == RPCStatusOK)
					status_code = (*rpc)(static_cast<TASK *>(task)->worker);
			}
		}
	}

	resp->set_status_code(status_code);
}

template<>
inline const RPCService *RPCServer<RPCTYPEThrift>::find_service(const std::string& name) const
{
	if (this->service_map.empty())
		return NULL;

	return this->service_map.cbegin()->second;
}

template<>
inline const RPCService *RPCServer<RPCTYPEThriftHttp>::find_service(const std::string& name) const
{
	if (this->service_map.empty())
		return NULL;

	return this->service_map.cbegin()->second;
}

} // namespace srpc

#endif

