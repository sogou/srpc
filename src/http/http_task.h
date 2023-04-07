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

#ifndef __RPC_HTTP_TASK_H__
#define __RPC_HTTP_TASK_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "workflow/HttpUtil.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFGlobal.h"
#include "rpc_module.h"

namespace srpc
{

// copy part of workflow/src/factory/HttpTaskImpl.cc

class HttpClientTask : public WFComplexClientTask<protocol::HttpRequest,
												  protocol::HttpResponse>
{
public:
	HttpClientTask(int redirect_max,
				   int retry_max,
				   http_callback_t&& callback,
				   std::list<RPCModule *>&& modules) :
		WFComplexClientTask(retry_max, std::move(callback)),
		redirect_max_(redirect_max),
		redirect_count_(0),
		modules_(std::move(modules))
	{
		protocol::HttpRequest *client_req = this->get_req();

		client_req->set_method(HttpMethodGet);
		client_req->set_http_version("HTTP/1.1");
	}

	RPCModuleData *mutable_module_data() { return &module_data_; }
	void set_module_data(RPCModuleData data) { module_data_ = std::move(data); }

/*
	// similar to opentracing: log({{"event", "error"}, {"message", "application log"}});
	void log(const RPCLogVector& fields);

	// Baggage Items, which are just key:value pairs that cross process boundaries
	void add_baggage(const std::string& key, const std::string& value);
	bool get_baggage(const std::string& key, std::string& value);
*/

protected:
	virtual CommMessageOut *message_out();
	virtual CommMessageIn *message_in();
	virtual int keep_alive_timeout();
	virtual bool init_success();
	virtual void init_failed();
	virtual bool finish_once();

protected:
	bool need_redirect(ParsedURI& uri);
	bool redirect_url(protocol::HttpResponse *client_resp, ParsedURI& uri);
	void set_empty_request();
	void check_response();

public:
	http_callback_t user_callback;

private:
	int redirect_max_;
	int redirect_count_;
	RPCModuleData module_data_;
	std::list<RPCModule *> modules_;
};

class HttpServerTask : public WFServerTask<protocol::HttpRequest,
										   protocol::HttpResponse>
{
public:
	HttpServerTask(CommService *service,
				   std::function<void (WFHttpTask *)>& process,
				   std::list<RPCModule *>&& modules) :
		WFServerTask(service, WFGlobal::get_scheduler(), process),
		req_is_alive_(false),
		req_has_keep_alive_header_(false),
		modules_(std::move(modules))
	{}

	class ModuleSeries : public WFServerTask<protocol::HttpRequest,
											 protocol::HttpResponse>::Series
	{
	public:
		ModuleSeries(WFServerTask<protocol::HttpRequest,
								  protocol::HttpResponse> *task) :
			WFServerTask<protocol::HttpRequest,
						 protocol::HttpResponse>::Series(task),
			module_data_(NULL)
		{}

		RPCModuleData *get_module_data() { return module_data_; }
		void set_module_data(RPCModuleData *data) { module_data_ = data; }
		bool has_module_data() const { return !!module_data_; }
		void clear_module_data() { module_data_ = NULL; }

	private:
		RPCModuleData *module_data_;
	};

//TODO:
//	bool get_remote(std::string& ip, unsigned short *port) const;

	RPCModuleData *mutable_module_data() { return &module_data_; }
	void set_module_data(RPCModuleData data) { module_data_ = std::move(data); }

protected:
	virtual void handle(int state, int error);
	virtual CommMessageOut *message_out();

protected:
	bool req_is_alive_;
	bool req_has_keep_alive_header_;
	std::string req_keep_alive_;
	RPCModuleData module_data_;
	std::list<RPCModule *> modules_;
};

} // end namespace srpc

#endif

