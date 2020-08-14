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

  Authors: Li Yingxin (liyingxin@sogou-inc.com)
           Wu Jiaxu (wujiaxu@sogou-inc.com)
*/

#include <string>
#include <functional>
#include <workflow/WFGlobal.h>
#include <workflow/WFTask.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFuture.h>
#include "rpc_basic.h"
#include "rpc_message.h"
#include "rpc_options.h"

namespace sogou
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
	int serialize_input(const ProtobufIDLMessage *in);
	int serialize_input(const ThriftIDLMessage *in);

protected:
	using user_done_t = std::function<int (int, RPCWorker&)>;

	using WFComplexClientTask<RPCREQ, RPCRESP>::get_req;
	using WFComplexClientTask<RPCREQ, RPCRESP>::get_resp;
	using WFComplexClientTask<RPCREQ, RPCRESP>::set_callback;

	void init_failed() override;
	bool check_request() override;
	CommMessageOut *message_out() override;
	bool finish_once() override;
	void rpc_callback(WFNetworkTask<RPCREQ, RPCRESP> *task);

public:
	RPCClientTask(const std::string& service_name,
				  const std::string& method_name,
				  const RPCTaskParams *params,
				  user_done_t&& user_done);

private:
	template<class IDL>
	int __serialize_input(const IDL *in);

	user_done_t user_done_;
	bool init_failed_;
};

template<class RPCREQ, class RPCRESP>
class RPCServerTask : public WFServerTask<RPCREQ, RPCRESP>
{
public:
	RPCServerTask(std::function<void (WFNetworkTask<RPCREQ, RPCRESP> *)>& process) :
		WFServerTask<RPCREQ, RPCRESP>(WFGlobal::get_scheduler(), process),
		worker(new RPCContextImpl<RPCREQ, RPCRESP>(this), &this->req, &this->resp)
	{}

	RPCWorker worker;

protected:
	CommMessageOut *message_out() override;
};

template<class OUTPUT>
static void RPCAsyncFutureCallback(OUTPUT *output, sogou::RPCContext *ctx)
{
	using RESULT = std::pair<OUTPUT, sogou::RPCSyncContext>;
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
static void ThriftSendCallback(OUTPUT *output, sogou::RPCContext *ctx)
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

template<class RPCREQ, class RPCRESP>
inline RPCClientTask<RPCREQ, RPCRESP>::RPCClientTask(
					const std::string& service_name,
					const std::string& method_name,
					const RPCTaskParams *params,
					user_done_t&& user_done):
	WFComplexClientTask<RPCREQ, RPCRESP>(0, nullptr),
	user_done_(std::move(user_done)),
	init_failed_(false)
{
	if (user_done_)
		this->set_callback(std::bind(&RPCClientTask::rpc_callback,
									 this, std::placeholders::_1));

	if (params->send_timeout != INT_UNSET)
		this->set_send_timeout(params->send_timeout);

	if (params->keep_alive_timeout != INT_UNSET)
		this->set_keep_alive(params->keep_alive_timeout);

	if (params->retry_max != INT_UNSET)
		this->set_retry_max(params->retry_max);

	if (params->compress_type != INT_UNSET)
		this->req.set_compress_type(params->compress_type);

	if (params->data_type != INT_UNSET)
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
	this->req.set_seqid(this->get_task_seq());
	int status_code = this->req.compress();

	if (status_code == RPCStatusOK)
	{
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

} // namespace sogou

