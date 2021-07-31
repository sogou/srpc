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

#ifndef __RPC_MESSAGE_BRPC_H__
#define __RPC_MESSAGE_BRPC_H__

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <arpa/inet.h>
#endif

#include "rpc_message.h"
#include "rpc_basic.h"

namespace srpc
{

static constexpr int BRPC_HEADER_SIZE = 12;

class BRPCMessage : public RPCMessage
{
public:
	BRPCMessage();
	virtual ~BRPCMessage();

	int encode(struct iovec vectors[], int max, size_t size_limit);
	int append(const void *buf, size_t *size, size_t size_limit);

	int get_compress_type() const override;
	void set_compress_type(int type) override;

	bool get_attachment_nocopy(const char **attachment, size_t *len) const;
	void set_attachment_nocopy(const char *attachment, size_t len);

	int get_data_type() const override { return RPCDataProtobuf; }
	void set_data_type(int type) override { }

	bool get_meta_module_data(RPCModuleData& data) const override { return false; }
	bool set_meta_module_data(const RPCModuleData& data) override { return false; }

public:
	using RPCMessage::serialize;
	using RPCMessage::deserialize;
	int serialize(const ProtobufIDLMessage *pb_msg) override;
	int deserialize(ProtobufIDLMessage *pb_msg) override;
	int compress() override;
	int decompress() override;

protected:
	// "PRPC" + PAYLOAD_SIZE + META_SIZE
	char header[BRPC_HEADER_SIZE];
	size_t nreceived;
	size_t meta_len;
	size_t message_len;
	size_t attachment_len;
	char *meta_buf;
	RPCBuffer *message;
	RPCBuffer *attachment;
	ProtobufIDLMessage *meta;
};

class BRPCRequest : public BRPCMessage
{
public:
	bool serialize_meta();
	bool deserialize_meta();

	const std::string& get_service_name() const;
	const std::string& get_method_name() const;

	void set_service_name(const std::string& service_name);
	void set_method_name(const std::string& method_name);

	int64_t get_correlation_id() const;
};

class BRPCResponse : public BRPCMessage
{
public:
	bool serialize_meta();
	bool deserialize_meta();

	int get_status_code() const;
	int get_error() const;
	const char *get_errmsg() const;

	void set_status_code(int code);
	void set_error(int error);

	void set_correlation_id(int64_t cid);

protected:
	int srpc_status_code = RPCStatusOK;
	std::string srpc_error_msg;
};

class BRPCStdRequest : public protocol::ProtocolMessage, public RPCRequest, public BRPCRequest
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->BRPCRequest::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->BRPCRequest::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->BRPCRequest::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->BRPCRequest::deserialize_meta();
	}

public:
	const std::string& get_service_name() const override
	{
		return this->BRPCRequest::get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return this->BRPCRequest::get_method_name();
	}

	void set_service_name(const std::string& service_name) override
	{
		return this->BRPCRequest::set_service_name(service_name);
	}

	void set_method_name(const std::string& method_name) override
	{
		return this->BRPCRequest::set_method_name(method_name);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->BRPCMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->BRPCMessage::get_meta_module_data(data);
	}

public:
	BRPCStdRequest() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class BRPCStdResponse : public protocol::ProtocolMessage, public RPCResponse, public BRPCResponse
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->BRPCResponse::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->BRPCResponse::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->BRPCResponse::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->BRPCResponse::deserialize_meta();
	}

public:
	int get_status_code() const override
	{
		return this->BRPCResponse::get_status_code();
	}

	int get_error() const override
	{
		return this->BRPCResponse::get_error();
	}

	const char *get_errmsg() const override
	{
		return this->BRPCResponse::get_errmsg();
	}

	void set_status_code(int code) override
	{
		return this->BRPCResponse::set_status_code(code);
	}

	void set_error(int error) override
	{
		return this->BRPCResponse::set_error(error);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->BRPCMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->BRPCMessage::get_meta_module_data(data);
	}

public:
	BRPCStdResponse() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

////////
// inl

inline BRPCMessage::~BRPCMessage()
{
	delete this->message;
	delete this->attachment;
	delete []this->meta_buf;
	delete this->meta;
}

inline int BRPCMessage::encode(struct iovec vectors[], int max, size_t size_limit)
{
	size_t sz = this->meta_len + this->message_len + this->attachment_len;

	if (sz > 0x7FFFFFFF)
	{
		errno = EOVERFLOW;
		return -1;
	}

	int ret;
	int total;
	char *p = this->header;

	memcpy(p, "PRPC", 4);
	p += 4;

	*(uint32_t *)(p) = htonl((uint32_t)sz);
	p += 4;

	*(uint32_t *)(p) = htonl((uint32_t)this->meta_len);

	vectors[0].iov_base = this->header;
	vectors[0].iov_len = BRPC_HEADER_SIZE;

	vectors[1].iov_base = this->meta_buf;
	vectors[1].iov_len = this->meta_len;

	ret = this->message->encode(vectors + 2, max - 2);
	if (ret < 0)
		return ret;

	total = ret;
	if (this->attachment_len)
	{
		ret = this->attachment->encode(vectors + 2 + ret, max - 2 - ret);
		if (ret < 0)
			return ret;

		total += ret;
	}

	return 2 + total;
}

} // namespace srpc

#endif

