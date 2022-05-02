#include <stdio.h>
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFHttpServer.h"
#include "rpc_filter_metrics.h"
#include "opentelemetry_metrics_service.pb.h"

namespace srpc
{
//TODO:
//using namespace opentelemetry::proto::collector::metrics::v1;
//using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;

RPCMetricsPull::RPCMetricsPull() :
	RPCFilter(RPCModuleTypeMetrics),
	server(std::bind(&RPCMetricsPull::pull,
					 this, std::placeholders::_1))
{
}

bool RPCMetricsPull::init(short port)
{
	if (this->server.start(port) == 0)
		return true;

	return false;
}

void RPCMetricsPull::deinit()
{
	this->server.stop();
}

void RPCMetricsPull::pull(WFHttpTask *task)
{
	if (strcmp(task->get_req()->get_request_uri(), "/metrics"))
		return;

	this->mutex.lock();
	this->expose();
	task->get_resp()->append_output_body(std::move(this->report_output));
	this->report_output.clear();
	this->mutex.unlock();
}

bool RPCMetricsPull::expose()
{
	std::unordered_map<std::string, RPCVar*> tmp;
	std::unordered_map<std::string, RPCVar*>::iterator it;
	std::string output;

	RPCVarGlobal *global_var = RPCVarGlobal::get_instance();

	global_var->mutex.lock();
	for (RPCVarLocal *local : global_var->local_vars)
	{
		local->mutex.lock();
		for (it = local->vars.begin(); it != local->vars.end(); it++)
		{
//			TODO:
//			if (it->first not in my_name_map)
//				continue;
			if (tmp.find(it->first) == tmp.end())
				tmp.insert(std::make_pair(it->first, it->second->create(true)));
			else
				tmp[it->first]->reduce(it->second->get_data(),
									   it->second->get_size());
		}
		local->mutex.unlock();
	}
	global_var->mutex.unlock();

	for (it = tmp.begin(); it != tmp.end(); it++)
	{
		RPCVar *var = it->second;
		this->report_output += "# HELP " + var->get_name() + " " +
							   var->get_help() + "\n# TYPE " + var->get_name() +
                               " " + var->get_type_str() + "\n";
		output.clear();
		var->collect(&output);
		this->report_output += output;
	}

	for (it = tmp.begin(); it != tmp.end(); it++)
		delete it->second;

	return true;
}

} // end namespace srpc

