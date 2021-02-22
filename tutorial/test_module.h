#include <signal.h>
#include "echo_pb.srpc.h"
#include "workflow/WFFacilities.h"
#include "srpc/rpc_module.h"
#include "srpc/rpc_span.h"
#include "srpc/rpc_types.h"

#ifndef __TEST_MODULE_H__
#define __TEST_MODULE_H__

namespace srpc
{

template<class RPCTYPE>
class ClientTestModule : public RPCClientModule<RPCTYPE>
{
public:

	using TASK = RPCClientTask<typename RPCTYPE::REQ, typename RPCTYPE::RESP>;

	int begin(TASK *task, const RPCModuleData& data) override
	{
		for (const auto pair : data)
			fprintf(stderr, "client module begin() get %s,%s\n",
					pair.first.c_str(), pair.second.c_str());

		std::string key = "trace_id";
		std::string value = "1412";
		fprintf(stderr, "this is client module. set kv[%s,%s] into meta\n",
				key.c_str(), value.c_str());
		RPCModuleData& module_data = task->mutable_module_data();
		module_data[key] = value;
		module_data["span_id"] = "2021";
		return 0;
	}

	int end(TASK *task, const RPCModuleData& data)override
	{
		for (const auto pair : data)
			fprintf(stderr, "client module end() get %s,%s on data\n",
					pair.first.c_str(), pair.second.c_str());
		for (const auto pair : task->mutable_module_data())
			fprintf(stderr, "client module end() get %s,%s on task\n",
					pair.first.c_str(), pair.second.c_str());
		return 0;
	}	
};

template<class RPCTYPE>
class ServerTestModule : public RPCServerModule<RPCTYPE>
{
public:
	using TASK = RPCServerTask<typename RPCTYPE::REQ, typename RPCTYPE::RESP>;

	int begin(TASK *task, const RPCModuleData& data) override
	{
		for (const auto pair : data)
			fprintf(stderr, "server module begin() get %s,%s\n",
					pair.first.c_str(), pair.second.c_str());

		std::string key = "workflow";
		std::string value = "srpc";
		fprintf(stderr, "this is server module. set kv[%s,%s] into meta\n",
				key.c_str(), value.c_str());
		RPCModuleData& module_data = task->mutable_module_data();
		module_data[key] = value;
		module_data["xiehan"] = "1412";
		return 0;
	}

	int end(TASK *task, const RPCModuleData& data) override
	{
		for (const auto pair : data)
			fprintf(stderr, "server module end() get %s,%s on data\n",
					pair.first.c_str(), pair.second.c_str());
		for (const auto pair : task->mutable_module_data())
			fprintf(stderr, "server module end() get %s,%s on task\n",
					pair.first.c_str(), pair.second.c_str());
	}
};

} // end namespace

#endif

