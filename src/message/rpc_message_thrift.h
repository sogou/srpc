/*
  Copyright (c) 2020 sogou, Inc.

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

#ifndef __RPC_MESSAGE_THRIFT_H__
#define __RPC_MESSAGE_THRIFT_H__

#include <string>
#include <workflow/HttpMessage.h>
#include "rpc_message.h"
#include "rpc_thrift_idl.h"

namespace srpc
{

class ThriftException : public ThriftIDLMessage
{
public:
	std::string message;
	int32_t type;

public:
	struct ISSET
	{
		bool message = true;
		bool type = true;
	}__isset;

	ThriftException()
	{
		this->elements = ThriftElementsImpl<ThriftException>::get_elements_instance();
		this->descriptor = ThriftDescriptorImpl<ThriftException, TDT_STRUCT, void, void>::get_instance();
	}

	static void StaticElementsImpl(std::list<struct_element> *elements)
	{
		const ThriftException *st = (const ThriftException *)0;
		const char *base = (const char *)st;
		using subtype_1 = ThriftDescriptorImpl<std::string, TDT_STRING, void, void>;
		using subtype_2 = ThriftDescriptorImpl<int32_t, TDT_I32, void, void>;

		elements->push_back({subtype_1::get_instance(), "message", (const char *)(&st->__isset.message) - base, (const char *)(&st->message) - base, 1});
		elements->push_back({subtype_2::get_instance(), "type", (const char *)(&st->__isset.type) - base, (const char *)(&st->type) - base, 2});
	}
};

class ThriftMessage : public RPCMessage
{
public:
	ThriftMessage() : TBuffer_(&buf_) { }
	virtual ~ThriftMessage() { }
	//copy constructor
	ThriftMessage(const ThriftMessage&) = delete;
	//copy operator
	ThriftMessage& operator= (const ThriftMessage&) = delete;
	//move constructor
	ThriftMessage(ThriftMessage&&) = delete;
	//move operator
	ThriftMessage& operator= (ThriftMessage&&) = delete;

public:
	int get_compress_type() const override { return RPCCompressNone; }
	int get_data_type() const override { return RPCDataThrift; }

	void set_compress_type(int type) override {}
	void set_data_type(int type) override {}

	void set_attachment_nocopy(const char *attachment, size_t len) { }
	bool get_attachment_nocopy(const char **attachment, size_t *len) const { return false; }

public:
	int serialize(const ThriftIDLMessage *thrift_msg) override;
	int deserialize(ThriftIDLMessage *thrift_msg) override;

	int compress() override { return RPCStatusOK; }
	int decompress() override { return RPCStatusOK; }

	bool get_meta_module_data(RPCModuleData& data) const override { return false; }
	bool set_meta_module_data(const RPCModuleData& data) override { return false; }

public:
	const ThriftMeta *get_meta() const { return &TBuffer_.meta; }
	ThriftMeta *get_meta() { return &TBuffer_.meta; }

protected:
	int encode(struct iovec vectors[], int max, size_t size_limit);
	int append(const void *buf, size_t *size, size_t size_limit);

	RPCBuffer buf_;
	ThriftBuffer TBuffer_;
};

class ThriftRequest : public ThriftMessage
{
public:
	bool serialize_meta() { return TBuffer_.writeMessageBegin(); }

	bool deserialize_meta() { return TBuffer_.readMessageBegin(); }

public:
	const std::string& get_service_name() const { return TBuffer_.meta.method_name; }
	const std::string& get_method_name() const { return TBuffer_.meta.method_name; }

	void set_service_name(const std::string& service_name) { }
	void set_method_name(const std::string& method_name) { TBuffer_.meta.method_name = method_name; }
	void set_seqid(long long seqid) { TBuffer_.meta.seqid = (int)seqid; }
};

class ThriftResponse : public ThriftMessage
{
public:
	bool serialize_meta();
	bool deserialize_meta();
	bool get_meta_module_data(RPCModuleData& data) const override { return false; }
	bool set_meta_module_data(const RPCModuleData& data) override { return false; }

public:
	int get_status_code() const { return status_code_; }
	int get_error() const { return error_; }
	const char *get_errmsg() const;

	void set_status_code(int code)
	{
		status_code_ = code;
	}

	void set_error(int error) { error_ = error; }

protected:
	int status_code_ = RPCStatusOK;
	int error_ = TET_UNKNOWN;
	std::string errmsg_;
};

class ThriftStdRequest : public protocol::ProtocolMessage, public RPCRequest, public ThriftRequest
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->ThriftRequest::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->ThriftRequest::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->ThriftRequest::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->ThriftRequest::deserialize_meta();
	}

public:
	const std::string& get_service_name() const override
	{
		return this->ThriftRequest::get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return this->ThriftRequest::get_method_name();
	}

	void set_service_name(const std::string& service_name) override
	{
		return this->ThriftRequest::set_service_name(service_name);
	}

	void set_method_name(const std::string& method_name) override
	{
		return this->ThriftRequest::set_method_name(method_name);
	}

	void set_seqid(long long seqid) override
	{
		this->ThriftRequest::set_seqid(seqid);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->ThriftMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->ThriftMessage::get_meta_module_data(data);
	}

public:
	ThriftStdRequest() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class ThriftStdResponse : public protocol::ProtocolMessage, public RPCResponse, public ThriftResponse
{
public:
	int encode(struct iovec vectors[], int max) override
	{
		return this->ThriftResponse::encode(vectors, max, this->size_limit);
	}

	int append(const void *buf, size_t *size) override
	{
		return this->ThriftResponse::append(buf, size, this->size_limit);
	}

public:
	bool serialize_meta() override
	{
		return this->ThriftResponse::serialize_meta();
	}

	bool deserialize_meta() override
	{
		return this->ThriftResponse::deserialize_meta();
	}

public:
	int get_status_code() const override
	{
		return this->ThriftResponse::get_status_code();
	}

	int get_error() const override
	{
		return this->ThriftResponse::get_error();
	}

	const char *get_errmsg() const override
	{
		return this->ThriftResponse::get_errmsg();
	}

	void set_status_code(int code) override
	{
		return this->ThriftResponse::set_status_code(code);
	}

	void set_error(int error) override
	{
		return this->ThriftResponse::set_error(error);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->ThriftMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->ThriftMessage::get_meta_module_data(data);
	}

public:
	ThriftStdResponse() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class ThriftHttpRequest : public protocol::HttpRequest, public RPCRequest, public ThriftRequest
{
public:
	bool serialize_meta() override;
	bool deserialize_meta() override;

public:
	const std::string& get_service_name() const override
	{
		return this->ThriftRequest::get_service_name();
	}

	const std::string& get_method_name() const override
	{
		return this->ThriftRequest::get_method_name();
	}

	void set_service_name(const std::string& service_name) override
	{
		return this->ThriftRequest::set_service_name(service_name);
	}

	void set_method_name(const std::string& method_name) override
	{
		return this->ThriftRequest::set_method_name(method_name);
	}

	void set_seqid(long long seqid) override
	{
		this->ThriftRequest::set_seqid(seqid);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->ThriftMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->ThriftMessage::get_meta_module_data(data);
	}

public:
	ThriftHttpRequest() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

class ThriftHttpResponse : public protocol::HttpResponse, public RPCResponse, public ThriftResponse
{
public:
	bool serialize_meta() override;
	bool deserialize_meta() override;

public:
	int get_status_code() const override
	{
		return this->ThriftResponse::get_status_code();
	}

	int get_error() const override
	{
		return this->ThriftResponse::get_error();
	}

	const char *get_errmsg() const override
	{
		return this->ThriftResponse::get_errmsg();
	}

	void set_status_code(int code) override
	{
		return this->ThriftResponse::set_status_code(code);
	}

	void set_error(int error) override
	{
		return this->ThriftResponse::set_error(error);
	}

	bool set_meta_module_data(const RPCModuleData& data) override
	{
		return this->ThriftMessage::set_meta_module_data(data);
	}

	bool get_meta_module_data(RPCModuleData& data) const override
	{
		return this->ThriftMessage::get_meta_module_data(data);
	}

public:
	ThriftHttpResponse() { this->size_limit = RPC_BODY_SIZE_LIMIT; }
};

////////
// inl

inline int ThriftMessage::serialize(const ThriftIDLMessage *thrift_msg)
{
	if (thrift_msg)
	{
		if (!thrift_msg->descriptor->writer(thrift_msg, &TBuffer_))
		{
			return TBuffer_.meta.message_type == TMT_CALL ? RPCStatusReqSerializeError
														  : RPCStatusRespSerializeError;
		}
	}

	return RPCStatusOK;
}

inline int ThriftMessage::deserialize(ThriftIDLMessage *thrift_msg)
{
	if (thrift_msg)
	{
		if (!thrift_msg->descriptor->reader(&TBuffer_, thrift_msg))
		{
			return TBuffer_.meta.message_type == TMT_CALL ? RPCStatusReqDeserializeError
														  : RPCStatusRespDeserializeError;
		}
	}

	return RPCStatusOK;
}

inline int ThriftMessage::encode(struct iovec vectors[], int max, size_t size_limit)
{
	size_t sz = TBuffer_.meta.writebuf.size() + buf_.size();

	if (sz > 0x7FFFFFFF)
	{
		errno = EOVERFLOW;
		return -1;
	}

	if (sz > 0)
	{
		TBuffer_.framesize = ntohl((int32_t)sz);
		vectors[0].iov_base = (char *)&TBuffer_.framesize;
		vectors[0].iov_len = sizeof (int32_t);
		vectors[1].iov_base = const_cast<char *>(TBuffer_.meta.writebuf.c_str());
		vectors[1].iov_len = TBuffer_.meta.writebuf.size();

		int ret = buf_.encode(vectors + 2, max - 2);

		return ret < 0 ? ret : 2 + ret;
	}

	return 0;
}

}

#endif

