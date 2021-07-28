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

#ifndef __RPC_MESSAGE_SRPC_H__
#define __RPC_MESSAGE_SRPC_H__

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <arpa/inet.h>
#endif

#include <workflow/HttpMessage.h>
#include "rpc_message.h"
#include "rpc_basic.h"
#include "rpc_thrift_idl.h"
#include "rpc_buffer.h"

namespace srpc
{

static constexpr int SRPC_HEADER_SIZE = 16;

// define sogou rpc protocol
class SRPCMessage : public RPCMessage
{
public:
	SRPCMessage();
	virtual ~SRPCMessage();

	int encode(struct iovec vectors[], int max, size_t size_limit);
	int append(const void *buf, size_t *size, size_t size_limit);

	bool serialize_meta();
	bool deserialize_meta();

	int get_compress_type() const override;
	int get_data_type() const override;

	void set_compress_type(int type) override;
	void set_data_type(int type) override;

	void set_attachment_nocopy(const char *attachment, size_t len);
	bool get_attachment_nocopy(const char **attachment, size_t *len) const;

	bool set_meta_module_data(const RPCModuleData& data) override;
	bool get_meta_module_data(RPCModuleData& data) const override;

public:
	using RPCMessage::serialize;
	using RPCMessage::deserialize;
	int serialize(const ProtobufIDLMessage *pb_msg) override;
	int deserialize(ProtobufIDLMessage *pb_msg) override;
	int serialize(const ThriftIDLMessage *thrift_msg) override;
	int deserialize(ThriftIDLMessage *thrift_msg) override;

	int compress() override;
	int decompress() override;

public:
	RPCBuffer *get_buffer() const { return this->buf; }
	size_t get_message_len() const { return this->message_len; }
	void set_message_len(size_t len) { this->message_len = len; }

protected:
	void init_meta();

	// "SRPC" + META_LEN + MESSAGE_LEN + RESERVED
	char header[SRPC_HEADER_SIZE];
	RPCBuffer *buf;
	char *meta_buf;
	size_t nreceived;
	size_t meta_len;
	size_t message_len;
	ProtobufIDLMessage *meta;
};

class SRPCRequest : public SRPCMessage
{
public:
	const std::string& get_service_name() const;
	const std::string& get_method_name() const;

	void set_service_name(const std::string& service_name);
	void set_method_name(const std::string& method_name);
};

class SRPCResponse : public SRPCMessage
{
public:
	int get_status_code() const;
	int get_error() const;
	const char *get_errmsg() const;

	void set_status_code(int code);
	void set_error(int error);
};

class SRPCStdRequest : public protocol::ProtocolMessage, public RPCRequest, public SRPCRequest
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->SRPCRequest::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->SRPCRequest::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->SRPCRequest::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->SRPCRequest::deserialize_meta();
	}

public:
	const std::string& get_service_name() const override
	{
		return this->SRPCRequest::get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return this->SRPCRequest::get_method_name();
	}

	void set_service_name(const std::string& service_name) override
	{
		return this->SRPCRequest::set_service_name(service_name);
	}

	void set_method_name(const std::string& method_name) override
	{
		return this->SRPCRequest::set_method_name(method_name);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->SRPCMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->SRPCMessage::get_meta_module_data(data);
	}

public:
	SRPCStdRequest() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class SRPCStdResponse : public protocol::ProtocolMessage, public RPCResponse, public SRPCResponse
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->SRPCResponse::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->SRPCResponse::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->SRPCResponse::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->SRPCResponse::deserialize_meta();
	}

public:
	int get_status_code() const override
	{
		return this->SRPCResponse::get_status_code();
	}

	int get_error() const override
	{
		return this->SRPCResponse::get_error();
	}

	const char *get_errmsg() const override
	{
		return this->SRPCResponse::get_errmsg();
	}

	void set_status_code(int code) override
	{
		return this->SRPCResponse::set_status_code(code);
	}

	void set_error(int error) override
	{
		return this->SRPCResponse::set_error(error);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->SRPCMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->SRPCMessage::get_meta_module_data(data);
	}

public:
	SRPCStdResponse() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class SRPCHttpRequest : public protocol::HttpRequest, public RPCRequest, public SRPCRequest
{
public:
	bool serialize_meta() override;
	bool deserialize_meta() override;

public:
	const std::string& get_service_name() const override
	{
		return this->SRPCRequest::get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return this->SRPCRequest::get_method_name();
	}

	void set_service_name(const std::string& service_name) override
	{
		return this->SRPCRequest::set_service_name(service_name);
	}

	void set_method_name(const std::string& method_name) override
	{
		return this->SRPCRequest::set_method_name(method_name);
	}

	bool set_meta_module_data(const RPCModuleData& data) override;
	bool get_meta_module_data(RPCModuleData& data) const override;

public:
	SRPCHttpRequest() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class SRPCHttpResponse : public protocol::HttpResponse, public RPCResponse, public SRPCResponse
{
public:
	bool serialize_meta() override;
	bool deserialize_meta() override;

public:
	int get_status_code() const override
	{
		return this->SRPCResponse::get_status_code();
	}

	int get_error() const override
	{
		return this->SRPCResponse::get_error();
	}

	const char *get_errmsg() const override
	{
		return this->SRPCResponse::get_errmsg();
	}

	void set_status_code(int code) override
	{
		return this->SRPCResponse::set_status_code(code);
	}

	void set_error(int error) override
	{
		return this->SRPCResponse::set_error(error);
	}

	bool set_meta_module_data(const RPCModuleData& data) override;
	bool get_meta_module_data(RPCModuleData& data) const override;

public:
	SRPCHttpResponse() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

////////
// inl

inline SRPCMessage::~SRPCMessage()
{
	delete []this->meta_buf;
	delete this->meta;
	delete this->buf;
}

inline int SRPCMessage::encode(struct iovec vectors[], int max, size_t size_limit)
{
	if (this->message_len > 0x7FFFFFFF)
	{
		errno = EOVERFLOW;
		return -1;
	}

	char *p = this->header;

	memcpy(p, "SRPC", 4);
	p += 4;

	*(uint32_t *)(p) = htonl((uint32_t)this->meta_len);
	p += 4;

	*(uint32_t *)(p) = htonl((uint32_t)this->message_len);

	vectors[0].iov_base = this->header;
	vectors[0].iov_len = SRPC_HEADER_SIZE;
	vectors[1].iov_base = this->meta_buf;
	vectors[1].iov_len = this->meta_len;

	int ret = this->buf->encode(vectors + 2, max - 2);

	return ret < 0 ? ret : ret + 2;
}

inline bool SRPCMessage::serialize_meta()
{
	this->meta_len = this->meta->ByteSize();
	this->meta_buf = new char[this->meta_len];
	return this->meta->SerializeToArray(this->meta_buf, (int)this->meta_len);
}

inline bool SRPCMessage::deserialize_meta()
{
	return this->meta->ParseFromArray(this->meta_buf, (int)this->meta_len);
}

} // namespace srpc

#endif

