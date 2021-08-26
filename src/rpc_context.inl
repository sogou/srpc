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

#include "rpc_message.h"
#include <workflow/WFTask.h>
#include <mutex>
#include <condition_variable>

namespace srpc
{

template<class T>
struct ThriftReceiver
{
	std::mutex mutex;
	std::condition_variable cond;
	RPCSyncContext ctx;
	T output;
	bool is_done = false;
};

template<class RPCREQ, class RPCRESP>
class RPCContextImpl : public RPCContext
{
public:
	long long get_seqid() const override
	{
		return task_->get_task_seq();
	}

	std::string get_remote_ip() const override
	{
		char ip_str[INET6_ADDRSTRLEN + 1] = { 0 };
		struct sockaddr_storage addr;
		socklen_t addrlen = sizeof (addr);

		if (this->get_peer_addr((struct sockaddr *)&addr, &addrlen) == 0)
		{
			if (addr.ss_family == AF_INET)
			{
				struct sockaddr_in *sin = (struct sockaddr_in *)(&addr);

				inet_ntop(AF_INET, &sin->sin_addr, ip_str, addrlen);
			}
			else if (addr.ss_family == AF_INET6)
			{
				struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)(&addr);

				inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, addrlen);
			}
		}

		return std::string(ip_str);
	}

	int get_peer_addr(struct sockaddr *addr, socklen_t *addrlen) const override
	{
		return task_->get_peer_addr(addr, addrlen);
	}

	const std::string& get_service_name() const override
	{
		return task_->get_req()->get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return task_->get_req()->get_method_name();
	}

	void set_data_type(RPCDataType type) override
	{
		task_->get_resp()->set_data_type(type);
	}

	void set_compress_type(RPCCompressType type) override
	{
		task_->get_resp()->set_compress_type(type);
	}

	void set_send_timeout(int timeout) override
	{
		task_->set_send_timeout(timeout);
	}

	void set_keep_alive(int timeout) override
	{
		task_->set_keep_alive(timeout);
	}

	SeriesWork *get_series() const override
	{
		return series_of(task_);
	}

	void *get_user_data() const override
	{
		return task_->user_data;
	}

	bool get_attachment(const char **attachment, size_t *len) const override
	{
		if (this->is_server_task())
			return task_->get_req()->get_attachment_nocopy(attachment, len);
		else
			return task_->get_resp()->get_attachment_nocopy(attachment, len);
	}

public:
	// for client-done
	bool success() const override
	{
		return task_->get_resp()->get_status_code() == RPCStatusOK;
	}

	int get_status_code() const override
	{
		return task_->get_resp()->get_status_code();
	}

	const char *get_errmsg() const override
	{
		return task_->get_resp()->get_errmsg();
	}

	int get_error() const override
	{
		return task_->get_resp()->get_error();
	}
	//int get_timeout_reason() const override;

public:
	// for server-process
	void set_attachment_nocopy(const char *attachment, size_t len) override
	{
		task_->get_resp()->set_attachment_nocopy(attachment, len);
	}

	void set_reply_callback(std::function<void (RPCContext *ctx)> cb) override
	{
		if (this->is_server_task())
		{
			if (cb)
			{
				task_->set_callback([this, cb](SubTask *task) {
					cb(this);
				});
			}
			else
				task_->set_callback(nullptr);
		}
	}

	void log(const RPCLogVector& fields) override { }

	void baggage(const std::string& key, const std::string& value) override { }

	//void noreply() override;
	//WFConnection *get_connection() override;

	RPCContextImpl(WFNetworkTask<RPCREQ, RPCRESP> *task) : task_(task) { }

protected:
	bool is_server_task() const
	{
		return task_->get_state() == WFT_STATE_TOREPLY
			|| task_->get_state() == WFT_STATE_NOREPLY;
	}

protected:
	WFNetworkTask<RPCREQ, RPCRESP> *task_;
};

} // namespace srpc

