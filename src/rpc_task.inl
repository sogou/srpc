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

#include <stdlib.h>
#include <string.h>
#include <string>
#include <functional>
#include <workflow/WFGlobal.h>
#include <workflow/WFTask.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFuture.h>
#include "rpc_basic.h"
#include "rpc_message.h"
#include "rpc_options.h"
#include "rpc_global.h"
#include "rpc_module.h"

namespace srpc
{

class RPCWorker
{
public:
	RPCWorker(RPCContext *ctx, RPCMessage *req, RPCMessage *resp)
	{
		this->ctx = ctx;
		this->req = req;
		this->resp = resp;
		this->__server_serialize = NULL;
	}

	~RPCWorker()
	{
		delete this->ctx;
		delete this->pb_input;
		delete this->pb_output;
		delete this->thrift_intput;
		delete this->thrift_output;
	}

	void set_server_input(ProtobufIDLMessage *input)
	{
		this->pb_input = input;
	}

	void set_server_input(ThriftIDLMessage *input)
	{
		this->thrift_intput = input;
	}

	void set_server_output(ProtobufIDLMessage *output)
	{
		this->pb_output = output;
		this->__server_serialize = &RPCWorker::resp_serialize_pb;
	}

	void set_server_output(ThriftIDLMessage *output)
	{
		this->thrift_output = output;
		this->__server_serialize = &RPCWorker::resp_serialize_thrift;
	}

	int server_serialize()
	{
		if (!this->__server_serialize)
			return RPCStatusOK;

		return (this->*__server_serialize)();
	}

public:
	RPCContext *ctx;
	RPCMessage *req;
	RPCMessage *resp;

private:
	int resp_serialize_pb()
	{
		return this->resp->serialize(this->pb_output);
	}

	int resp_serialize_thrift()
	{
		return this->resp->serialize(this->thrift_output);
	}

	int (RPCWorker::*__server_serialize)();
	ProtobufIDLMessage *pb_input = NULL;
	ProtobufIDLMessage *pb_output = NULL;
	ThriftIDLMessage *thrift_intput = NULL;
	ThriftIDLMessage *thrift_output = NULL;
};

template<class RPCREQ, class RPCRESP>
class RPCClientTask : public WFComplexClientTask<RPCREQ, RPCRESP>
{
public:
	// before rpc call
	void set_data_type(RPCDataType type);
	void set_compress_type(RPCCompressType type);
	void set_retry_max(int retry_max);
	void set_attachment_nocopy(const char *attachment, size_t len);
	int set_uri_fragment(const std::string& fragment);
	int serialize_input(const ProtobufIDLMessage *in);
	int serialize_input(const ThriftIDLMessage *in);

	// similar to opentracing: log({{"event", "error"}, {"message", "application log"}});
	void log(const RPCLogVector& fields);

	// Baggage Items, which are just key:value pairs that cross process boundaries
	void baggage(const std::string& key, const std::string& value);

protected:
	using user_done_t = std::function<int (int, RPCWorker&)>;

	using WFComplexClientTask<RPCREQ, RPCRESP>::set_callback;

	void init_failed() override;
	bool check_request() override;
	CommMessageOut *message_out() override;
	bool finish_once() override;
	void rpc_callback(WFNetworkTask<RPCREQ, RPCRESP> *task);
	int first_timeout() override { return watch_timeout_; }

public:
	RPCClientTask(const std::string& service_name,
				  const std::string& method_name,
				  const RPCTaskParams *params,
				  const std::list<RPCModule *>&& modules,
				  user_done_t&& user_done);

	bool get_remote(std::string& ip, unsigned short *port) const;

	RPCModuleData *mutable_module_data() { return &module_data_; }
	void set_module_data(RPCModuleData data) { module_data_ = std::move(data); }

private:
	template<class IDL>
	int __serialize_input(const IDL *in);

	user_done_t user_done_;
	bool init_failed_;
	int watch_timeout_;

	RPCModuleData module_data_;
	std::list<RPCModule *> modules_;
};

template<class RPCREQ, class RPCRESP>
class RPCServerTask : public WFServerTask<RPCREQ, RPCRESP>
{
public:
	RPCServerTask(CommService *service,
				  std::function<void (WFNetworkTask<RPCREQ, RPCRESP> *)>& process,
				  const std::list<RPCModule *>&& modules) :
		WFServerTask<RPCREQ, RPCRESP>(service, WFGlobal::get_scheduler(), process),
		worker(new RPCContextImpl<RPCREQ, RPCRESP>(this), &this->req, &this->resp),
		modules_(modules)
	{
	}

public:
	class RPCSeries : public WFServerTask<RPCREQ, RPCRESP>::Series
	{
	public:
		RPCSeries(WFServerTask<RPCREQ, RPCRESP> *task) :
			WFServerTask<RPCREQ, RPCRESP>::Series(task),
			module_data(NULL)
		{}

		RPCModuleData *get_module_data() { return this->module_data; }
		void set_module_data(RPCModuleData *data) { this->module_data = data; }
		bool has_module_data() const { return !!this->module_data; }
		void clear_module_data() { this->module_data = NULL; }

	private:
		RPCModuleData *module_data;
	};

protected:
	CommMessageOut *message_out() override;
	void handle(int state, int error) override;

public:
	bool get_remote(std::string& ip, unsigned short *port) const;
	RPCModuleData *mutable_module_data() { return &module_data_; }
	void set_module_data(RPCModuleData data) { module_data_ = std::move(data); }
	void log(const RPCLogVector& fields);
	void baggage(const std::string& key, const std::string& value);

public:
	RPCWorker worker;

private:
	RPCModuleData module_data_;
	std::list<RPCModule *> modules_;
};

template<class OUTPUT>
static void RPCAsyncFutureCallback(OUTPUT *output, srpc::RPCContext *ctx)
{
	using RESULT = std::pair<OUTPUT, srpc::RPCSyncContext>;
	auto *pr = static_cast<WFPromise<RESULT> *>(ctx->get_user_data());
	RESULT res;

	res.second.seqid = ctx->get_seqid();
	res.second.errmsg = ctx->get_errmsg();
	res.second.remote_ip = ctx->get_remote_ip();
	res.second.status_code = ctx->get_status_code();
	res.second.error = ctx->get_error();
	res.second.success = ctx->success();
	if (res.second.success)
		res.first = std::move(*output);

	pr->set_value(std::move(res));
	delete pr;
}

template<class OUTPUT>
static void ThriftSendCallback(OUTPUT *output, srpc::RPCContext *ctx)
{
	auto *receiver = static_cast<ThriftReceiver<OUTPUT> *>(ctx->get_user_data());

	receiver->mutex.lock();
	receiver->ctx.seqid = ctx->get_seqid();
	receiver->ctx.errmsg = ctx->get_errmsg();
	receiver->ctx.remote_ip = ctx->get_remote_ip();
	receiver->ctx.status_code = ctx->get_status_code();
	receiver->ctx.error = ctx->get_error();
	receiver->ctx.success = ctx->success();
	if (receiver->ctx.success)
		receiver->output = std::move(*output);

	receiver->is_done = true;
	receiver->cond.notify_one();
	receiver->mutex.unlock();
}

template<class OUTPUT>
static inline int
ClientRPCDoneImpl(int status_code,
				  RPCWorker& worker,
				  const std::function<void (OUTPUT *, RPCContext *)>& rpc_done)
{
	if (status_code == RPCStatusOK)
	{
		OUTPUT out;

		status_code = worker.resp->deserialize(&out);
		if (status_code == RPCStatusOK)
			rpc_done(&out, worker.ctx);

		return status_code;
	}

	rpc_done(NULL, worker.ctx);
	return status_code;
}

template<class RPCREQ, class RPCRESP>
CommMessageOut *RPCServerTask<RPCREQ, RPCRESP>::message_out()
{
	int status_code = this->worker.server_serialize();

	if (status_code == RPCStatusOK)
	{
		status_code = this->resp.compress();
		if (status_code == RPCStatusOK)
		{
			RPCSeries *series = static_cast<RPCSeries *>(series_of(this));

			RPCModuleData *data = series->get_module_data();

			if (data != NULL)
				this->set_module_data(std::move(*data));
			else
				data = &global_empty_map;

			for (auto *module : modules_)
				module->server_task_end(this, *data);

			this->resp.set_meta_module_data(*this->mutable_module_data());
			series->clear_module_data();

			if (this->resp.serialize_meta())
				return this->WFServerTask<RPCREQ, RPCRESP>::message_out();

			status_code = RPCStatusMetaError;
		}
	}

	this->resp.set_status_code(status_code);
	errno = EBADMSG;
	return NULL;
}

template<class RPCREQ, class RPCRESP>
void RPCServerTask<RPCREQ, RPCRESP>::handle(int state, int error)
{
	if (state != WFT_STATE_TOREPLY)
		return WFServerTask<RPCREQ, RPCRESP>::handle(state, error);

	this->state = WFT_STATE_TOREPLY;
	this->target = this->get_target();
	RPCSeries *series = new RPCSeries(this);
	series->start();
}

template<class RPCREQ, class RPCRESP>
inline void RPCClientTask<RPCREQ, RPCRESP>::set_data_type(RPCDataType type)
{
	this->req.set_data_type(type);
}

template<class RPCREQ, class RPCRESP>
inline void RPCClientTask<RPCREQ, RPCRESP>::set_compress_type(RPCCompressType type)
{
	this->req.set_compress_type(type);
}

template<class RPCREQ, class RPCRESP>
inline void RPCClientTask<RPCREQ, RPCRESP>::set_attachment_nocopy(const char *attachment,
																  size_t len)
{
	this->req.set_attachment_nocopy(attachment, len);
}

template<class RPCREQ, class RPCRESP>
int RPCClientTask<RPCREQ, RPCRESP>::set_uri_fragment(const std::string& fragment)
{
	char *str = strdup(fragment.c_str());

	if (str)
	{
		free(this->uri_.fragment);
		this->uri_.fragment = str;
		return 0;
	}

	return -1;
}

template<class RPCREQ, class RPCRESP>
inline void RPCClientTask<RPCREQ, RPCRESP>::set_retry_max(int retry_max)
{
	this->retry_max_ = retry_max;
}

template<class RPCREQ, class RPCRESP>
inline int RPCClientTask<RPCREQ, RPCRESP>::serialize_input(const ProtobufIDLMessage *in)
{
	return __serialize_input<ProtobufIDLMessage>(in);
}

template<class RPCREQ, class RPCRESP>
inline int RPCClientTask<RPCREQ, RPCRESP>::serialize_input(const ThriftIDLMessage *in)
{
	return __serialize_input<ThriftIDLMessage>(in);
}

template<class RPCREQ, class RPCRESP>
template<class IDL>
inline int RPCClientTask<RPCREQ, RPCRESP>::__serialize_input(const IDL *in)
{
	if (init_failed_ == false)
	{
		int status_code = this->req.serialize(in);

		this->resp.set_status_code(status_code);
		if (status_code == RPCStatusOK)
			return 0;
	}

	return -1;
}

static bool addr_to_string(const struct sockaddr *addr,
						   char *ip_str, socklen_t len,
						   unsigned short *port)
{
	const char *ret = NULL;

	if (addr->sa_family == AF_INET)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;

		ret = inet_ntop(AF_INET, &sin->sin_addr, ip_str, len);
		*port = ntohs(sin->sin_port);
	}
	else if (addr->sa_family == AF_INET6)
	{
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;

		ret = inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, len);
		*port = ntohs(sin6->sin6_port);
	}
	else
		errno = EINVAL;

	return ret != NULL;
}

template<class RPCREQ, class RPCRESP>
inline RPCClientTask<RPCREQ, RPCRESP>::RPCClientTask(
					const std::string& service_name,
					const std::string& method_name,
					const RPCTaskParams *params,
					const std::list<RPCModule *>&& modules,
					user_done_t&& user_done):
	WFComplexClientTask<RPCREQ, RPCRESP>(0, nullptr),
	user_done_(std::move(user_done)),
	init_failed_(false),
	modules_(modules)
{
	if (user_done_)
		this->set_callback(std::bind(&RPCClientTask::rpc_callback,
									 this, std::placeholders::_1));

	this->set_send_timeout(params->send_timeout);
	this->set_receive_timeout(params->receive_timeout);
	watch_timeout_ = params->watch_timeout;
	this->set_keep_alive(params->keep_alive_timeout);
	this->set_retry_max(params->retry_max);

	if (params->compress_type != RPCCompressNone)
		this->req.set_compress_type(params->compress_type);

	if (params->data_type != RPCDataUndefined)
		this->req.set_data_type(params->data_type);

	this->req.set_service_name(service_name);
	this->req.set_method_name(method_name);
}

template<class RPCREQ, class RPCRESP>
void RPCClientTask<RPCREQ, RPCRESP>::init_failed()
{
	init_failed_ = true;
}

template<class RPCREQ, class RPCRESP>
bool RPCClientTask<RPCREQ, RPCRESP>::check_request()
{
	int status_code = this->resp.get_status_code();

	return status_code == RPCStatusOK || status_code == RPCStatusUndefined;
}

template<class RPCREQ, class RPCRESP>
CommMessageOut *RPCClientTask<RPCREQ, RPCRESP>::message_out()
{
	using SERIES = typename RPCServerTask<RPCREQ, RPCRESP>::RPCSeries;
	this->req.set_seqid(this->get_task_seq());

	int status_code = this->req.compress();

	if (status_code == RPCStatusOK)
	{
		RPCModuleData *data = NULL;
		SERIES *series = dynamic_cast<SERIES *>(series_of(this));

		if (series)
			data = series->get_module_data();

		if (data != NULL)
			this->set_module_data(*data);
		else
			data = &global_empty_map;

		for (auto *module : modules_)
			module->client_task_begin(this, *data);

		this->req.set_meta_module_data(*this->mutable_module_data());

		if (this->req.serialize_meta())
			return this->WFClientTask<RPCREQ, RPCRESP>::message_out();

		status_code = RPCStatusMetaError;
	}

	this->disable_retry();
	this->resp.set_status_code(status_code);
	errno = EBADMSG;
	return NULL;
}

template<class RPCREQ, class RPCRESP>
bool RPCClientTask<RPCREQ, RPCRESP>::finish_once()
{
	int status_code = this->resp.get_status_code();

	if (this->state == WFT_STATE_SUCCESS &&
		(status_code == RPCStatusOK || status_code == RPCStatusUndefined))
	{
		if (this->resp.deserialize_meta() == false)
			this->resp.set_status_code(RPCStatusMetaError);

		if (!modules_.empty())
		{
			RPCModuleData resp_data;
			this->resp.get_meta_module_data(resp_data);
			for (auto *module : modules_)
				module->client_task_end(this, resp_data);
		}
		//TODO: Feedback client resp meta through all nodes by series
	}

	return true;
}

template<class RPCREQ, class RPCRESP>
void RPCClientTask<RPCREQ, RPCRESP>::rpc_callback(WFNetworkTask<RPCREQ, RPCRESP> *task)
{
	RPCWorker worker(new RPCContextImpl<RPCREQ, RPCRESP>(this),
					 &this->req, &this->resp);
	int status_code = this->resp.get_status_code();

	if (status_code != RPCStatusOK && status_code != RPCStatusUndefined)
	{
		this->state = WFT_STATE_TASK_ERROR;
		this->error = status_code;
	}
	else if (this->state == WFT_STATE_SUCCESS)
	{
		status_code = this->resp.decompress();
		if (status_code == RPCStatusOK)
		{
			this->resp.set_status_code(RPCStatusOK);
			status_code = user_done_(status_code, worker);
			if (status_code == RPCStatusOK)
				return;
		}

		this->state = WFT_STATE_TASK_ERROR;
		this->error = status_code;
	}

	if (this->state == WFT_STATE_TASK_ERROR)
	{
		switch (this->error)
		{
		case WFT_ERR_URI_PARSE_FAILED:
		case WFT_ERR_URI_SCHEME_INVALID:
		case WFT_ERR_URI_PORT_INVALID:
			status_code = RPCStatusURIInvalid;
			break;
		case WFT_ERR_UPSTREAM_UNAVAILABLE:
			status_code = RPCStatusUpstreamFailed;
			break;
		default:
			break;
		}
	}
	else
	{
		switch (this->state)
		{
		case WFT_STATE_SYS_ERROR:
			status_code = RPCStatusSystemError;
			break;
		case WFT_STATE_SSL_ERROR:
			status_code = RPCStatusSSLError;
			break;
		case WFT_STATE_DNS_ERROR:
			status_code = RPCStatusDNSError;
			break;
		case WFT_STATE_ABORTED:
			status_code = RPCStatusProcessTerminated;
			break;
		default:
			status_code = RPCStatusUndefined;
			break;
		}
	}

	this->resp.set_status_code(status_code);
	this->resp.set_error(this->error);

	user_done_(status_code, worker);
}

template<class RPCREQ, class RPCRESP>
bool RPCClientTask<RPCREQ, RPCRESP>::get_remote(std::string& ip,
												unsigned short *port) const
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof (addr);

	ip.resize(INET6_ADDRSTRLEN + 1);

	if (this->get_peer_addr((struct sockaddr *)&addr, &addrlen) == 0)
		return addr_to_string((struct sockaddr *)&addr, (char *)ip.c_str(),
							  INET6_ADDRSTRLEN + 1, port);

	return false;
}

template<class RPCREQ, class RPCRESP>
bool RPCServerTask<RPCREQ, RPCRESP>::get_remote(std::string& ip,
												unsigned short *port) const
{
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof (addr);

	ip.resize(INET6_ADDRSTRLEN + 1);

	if (this->get_peer_addr((struct sockaddr *)&addr, &addrlen) == 0)
		return addr_to_string((struct sockaddr *)&addr, (char *)ip.c_str(),
							  INET6_ADDRSTRLEN + 1, port);

	return false;
}

static void log_format(std::string& key, std::string& value,
					   const RPCLogVector& fields)
{
	if (fields.size() == 0)
		return;

	char buffer[100];
	snprintf(buffer, 100, "%s%c%lld", SRPC_SPAN_LOG, ' ', GET_CURRENT_MS());
	key = std::move(buffer);
	value = "{\"";

	for (auto& field : fields)
	{
		value = value + std::move(field.first) + "\":\""
			  + std::move(field.second) + "\",";
	}

	value[value.length() - 1] = '}';
}

template<class RPCREQ, class RPCRESP>
void RPCClientTask<RPCREQ, RPCRESP>::log(const RPCLogVector& fields)
{
	std::string key;
	std::string value;
	log_format(key, value, fields);
	module_data_.insert(std::make_pair(std::move(key), std::move(value)));
}

template<class RPCREQ, class RPCRESP>
void RPCClientTask<RPCREQ, RPCRESP>::baggage(const std::string& key,
											 const std::string& value)
{
	module_data_.insert(std::make_pair(std::move(key), std::move(value)));
}

template<class RPCREQ, class RPCRESP>
void RPCServerTask<RPCREQ, RPCRESP>::log(const RPCLogVector& fields)
{
	std::string key;
	std::string value;
	log_format(key, value, fields);
	module_data_.insert(std::make_pair(std::move(key), std::move(value)));
}

template<class RPCREQ, class RPCRESP>
void RPCServerTask<RPCREQ, RPCRESP>::baggage(const std::string& key,
											 const std::string& value)
{
	module_data_.insert(std::make_pair(std::move(key), std::move(value)));
}

} // namespace srpc

