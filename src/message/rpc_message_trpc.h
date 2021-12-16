/*
  Copyright (c) 2021 Sogou, Inc.

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

#ifndef __RPC_MESSAGE_TRPC_H__
#define __RPC_MESSAGE_TRPC_H__

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <arpa/inet.h>
#endif

#include <workflow/HttpMessage.h>
#include "rpc_message.h"
#include "rpc_basic.h"

namespace srpc
{

static constexpr int TRPC_HEADER_SIZE = 16;

class TRPCMessage : public RPCMessage
{
public:
	TRPCMessage();
	virtual ~TRPCMessage();

	int encode(struct iovec vectors[], int max, size_t size_limit);
	int append(const void *buf, size_t *size, size_t size_limit);

	bool get_attachment_nocopy(const char **attachment, size_t *len) const { return false; }
	void set_attachment_nocopy(const char *attachment, size_t len) { }

public:
	using RPCMessage::serialize;
	using RPCMessage::deserialize;
	int serialize(const ProtobufIDLMessage *pb_msg) override;
	int deserialize(ProtobufIDLMessage *pb_msg) override;
	int compress() override;
	int decompress() override;

protected:
	char header[TRPC_HEADER_SIZE];
	size_t nreceived;
	size_t meta_len;
	size_t message_len;
	char *meta_buf;
	RPCBuffer *message;
	ProtobufIDLMessage *meta;

	int compress_type_trpc_srpc(int trpc_content_encoding) const;
	int compress_type_srpc_trpc(int srpc_compress_type) const;
	int data_type_trpc_srpc(int trpc_content_type) const;
	int data_type_srpc_trpc(int srpc_data_type) const;
	int status_code_srpc_trpc(int srpc_status_code) const;
	const char *error_msg_srpc_trpc(int srpc_status_code) const;

	RPCBuffer *get_buffer() const { return this->message; }
};

class TRPCRequest : public TRPCMessage
{
public:
	TRPCRequest();

	bool serialize_meta();
	bool deserialize_meta();

	const std::string& get_service_name() const;
	const std::string& get_method_name() const;
	const std::string& get_caller_name() const;

	void set_service_name(const std::string& service_name);
	void set_method_name(const std::string& method_name);
	void set_caller_name(const std::string& caller_name);

	int get_compress_type() const override;
	void set_compress_type(int type) override;

	int get_data_type() const override;
	void set_data_type(int type) override;

	void set_request_id(int32_t req_id);
	int32_t get_request_id() const;

	bool set_meta_module_data(const RPCModuleData& data) override;
	bool get_meta_module_data(RPCModuleData& data) const override;

	bool trim_method_prefix();
};

class TRPCResponse : public TRPCMessage
{
public:
	TRPCResponse();

	bool serialize_meta();
	bool deserialize_meta();

	int get_compress_type() const override;
	void set_compress_type(int type) override;

	int get_data_type() const override;
	void set_data_type(int type) override;

	int get_status_code() const;
	int get_error() const;
	const char *get_errmsg() const;

	void set_status_code(int code);
	void set_error(int error);

	void set_request_id(int32_t req_id);
	int32_t get_request_id() const;

	bool set_meta_module_data(const RPCModuleData& data) override;
	bool get_meta_module_data(RPCModuleData& data) const override;

protected:
	int srpc_status_code = RPCStatusOK;
	std::string srpc_error_msg;
};

class TRPCStdRequest : public protocol::ProtocolMessage, public RPCRequest, public TRPCRequest
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->TRPCRequest::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->TRPCRequest::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->TRPCRequest::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->TRPCRequest::deserialize_meta();
	}

public:
	const std::string& get_service_name() const override
	{
		return this->TRPCRequest::get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return this->TRPCRequest::get_method_name();
	}

	void set_service_name(const std::string& service_name) override
	{
		return this->TRPCRequest::set_service_name(service_name);
	}

	void set_method_name(const std::string& method_name) override
	{
		return this->TRPCRequest::set_method_name(method_name);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->TRPCRequest::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->TRPCRequest::get_meta_module_data(data);
	}

	void set_caller_name(const std::string& caller_name)
	{
		return this->TRPCRequest::set_caller_name(caller_name);
	}

public:
	TRPCStdRequest() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class TRPCStdResponse : public protocol::ProtocolMessage, public RPCResponse, public TRPCResponse
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->TRPCResponse::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->TRPCResponse::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->TRPCResponse::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->TRPCResponse::deserialize_meta();
	}

public:
	int get_status_code() const override
	{
		return this->TRPCResponse::get_status_code();
	}

	int get_error() const override
	{
		return this->TRPCResponse::get_error();
	}

	const char *get_errmsg() const override
	{
		return this->TRPCResponse::get_errmsg();
	}

	void set_status_code(int code) override
	{
		return this->TRPCResponse::set_status_code(code);
	}

	void set_error(int error) override
	{
		return this->TRPCResponse::set_error(error);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->TRPCResponse::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->TRPCResponse::get_meta_module_data(data);
	}

public:
	TRPCStdResponse() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class TRPCHttpRequest : public protocol::HttpRequest, public RPCRequest, public TRPCRequest
{
public:
	bool serialize_meta() override;
	bool deserialize_meta() override;

public:
	const std::string& get_service_name() const override
	{
		return this->TRPCRequest::get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return this->TRPCRequest::get_method_name();
	}

	void set_service_name(const std::string& service_name) override
	{
		return this->TRPCRequest::set_service_name(service_name);
	}

	void set_method_name(const std::string& method_name) override
	{
		return this->TRPCRequest::set_method_name(method_name);
	}

	bool set_meta_module_data(const RPCModuleData& data) override;
	bool get_meta_module_data(RPCModuleData& data) const override;

	void set_caller_name(const std::string& caller_name)
	{
		return this->TRPCRequest::set_caller_name(caller_name);
	}

	const std::string& get_caller_name() const
	{
		return this->TRPCRequest::get_caller_name();
	}

public:
	TRPCHttpRequest() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class TRPCHttpResponse : public protocol::HttpResponse, public RPCResponse, public TRPCResponse
{
public:
	bool serialize_meta() override;
	bool deserialize_meta() override;

public:
	int get_status_code() const override
	{
		return this->TRPCResponse::get_status_code();
	}

	int get_error() const override
	{
		return this->TRPCResponse::get_error();
	}

	const char *get_errmsg() const override
	{
		return this->TRPCResponse::get_errmsg();
	}

	void set_status_code(int code) override
	{
		return this->TRPCResponse::set_status_code(code);
	}

	void set_error(int error) override
	{
		return this->TRPCResponse::set_error(error);
	}

	bool set_meta_module_data(const RPCModuleData& data) override;
	bool get_meta_module_data(RPCModuleData& data) const override;

public:
	TRPCHttpResponse() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

} // namespace srpc

#endif

