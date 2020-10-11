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

#ifndef __RPC_SERVICE_H__
#define __RPC_SERVICE_H__

#include <string>
#include <unordered_map>
#include <functional>
#include "rpc_context.h"
#include "rpc_options.h"

namespace srpc
{

class RPCService
{
protected:
	using rpc_method_t = std::function<int (RPCWorker&)>;

public:
	RPCService(const std::string& name) : name_(name) { }
	RPCService(RPCService&& move) = delete;
	RPCService& operator=(RPCService&& move) = delete;
	RPCService(const RPCService& copy) = delete;
	RPCService& operator=(const RPCService& copy) = delete;
	virtual ~RPCService() { };

	const std::string& get_name() const { return name_; }
	const rpc_method_t *find_method(const std::string& method_name) const;

protected:
	void add_method(const std::string& method_name, rpc_method_t&& method);

private:
	std::unordered_map<std::string, rpc_method_t> methods_;
	std::string name_;
};

////////
// inl

template<class INPUT, class OUTPUT, class SERVICE>
static inline int
ServiceRPCCallImpl(SERVICE *service,
				   RPCWorker& worker,
				   void (SERVICE::*rpc)(INPUT *, OUTPUT *, RPCContext *))
{
	auto *in = new INPUT;

	worker.set_server_input(in);
	int status_code = worker.req->deserialize(in);

	if (status_code == RPCStatusOK)
	{
		auto *out = new OUTPUT;

		worker.set_server_output(out);
		(service->*rpc)(in, out, worker.ctx);
	}

	return status_code;
}

inline void RPCService::add_method(const std::string& method_name, rpc_method_t&& method)
{
	methods_.emplace(method_name, std::move(method));
}

inline const RPCService::rpc_method_t *RPCService::find_method(const std::string& method_name) const
{
	const auto it = methods_.find(method_name);

	if (it != methods_.cend())
		return &it->second;

	return NULL;
}

} // namespace srpc

#endif

