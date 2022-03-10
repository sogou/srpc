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

#ifndef __RPC_MESSAGE_H__
#define __RPC_MESSAGE_H__

#include <string.h>
#include <string>
#include <workflow/ProtocolMessage.h>
#include "rpc_basic.h"
#include "rpc_filter.h"
#include "rpc_thrift_idl.h"

namespace srpc
{

class RPCRequest
{
public:
	virtual bool serialize_meta() = 0;
	virtual bool deserialize_meta() = 0;

	virtual const std::string& get_service_name() const = 0;
	virtual const std::string& get_method_name() const = 0;

	virtual void set_service_name(const std::string& service_name) = 0;
	virtual void set_method_name(const std::string& method_name) = 0;

	virtual bool get_meta_module_data(RPCModuleData& data) const = 0;
	virtual bool set_meta_module_data(const RPCModuleData& data) = 0;

	virtual void set_seqid(long long seqid) {}
};

class RPCResponse
{
public:
	virtual bool serialize_meta() = 0;
	virtual bool deserialize_meta() = 0;

	virtual int get_status_code() const = 0;
	virtual int get_error() const = 0;
	virtual const char *get_errmsg() const = 0;

	virtual void set_status_code(int code) = 0;
	virtual void set_error(int error) = 0;

	virtual bool set_http_code(const char *code) { return false; }
};

class RPCMessage
{
public:
	virtual void set_data_type(int type) = 0;
	virtual void set_compress_type(int type) = 0;
	virtual int get_compress_type() const = 0;
	virtual int get_data_type() const = 0;

public:
	//return RPCStatus
	virtual int compress() = 0;
	virtual int decompress() = 0;
	virtual bool get_meta_module_data(RPCModuleData& data) const = 0;
	virtual bool set_meta_module_data(const RPCModuleData& data) = 0;

	void set_json_add_whitespace(bool on);
	bool get_json_add_whitespace() const;
	void set_json_enums_as_ints(bool on);
	bool get_json_enums_as_ints() const;
	void set_json_preserve_names(bool on);
	bool get_json_preserve_names() const;
	void set_json_print_primitive(bool on);
	bool get_json_print_primitive() const;

public:
	//pb
	virtual int serialize(const ProtobufIDLMessage *idl_msg)
	{
		return RPCStatusIDLSerializeNotSupported;
	}

	virtual int deserialize(ProtobufIDLMessage *idl_msg)
	{
		return RPCStatusIDLDeserializeNotSupported;
	}

public:
	//thrift
	virtual int serialize(const ThriftIDLMessage *idl_msg)
	{
		return RPCStatusIDLSerializeNotSupported;
	}

	virtual int deserialize(ThriftIDLMessage *idl_msg)
	{
		return RPCStatusIDLDeserializeNotSupported;
	}

public:
	RPCMessage() { this->flags = 0; }

protected:
	uint32_t flags;
};


// implementation

inline void RPCMessage::set_json_add_whitespace(bool on)
{
	if (on)
		this->flags |= SRPC_JSON_OPTION_ADD_WHITESPACE;
	else
		this->flags &= ~SRPC_JSON_OPTION_ADD_WHITESPACE;
}

inline bool RPCMessage::get_json_add_whitespace() const
{
	return this->flags & SRPC_JSON_OPTION_ADD_WHITESPACE;
}

inline void RPCMessage::set_json_enums_as_ints(bool on)
{
	if (on)
		this->flags |= SRPC_JSON_OPTION_ENUM_AS_INITS;
	else
		this->flags &= ~SRPC_JSON_OPTION_ENUM_AS_INITS;
}

inline bool RPCMessage::get_json_enums_as_ints() const
{
	return this->flags & SRPC_JSON_OPTION_ENUM_AS_INITS;
}

inline void RPCMessage::set_json_preserve_names(bool on)
{
	if (on)
		this->flags |= SRPC_JSON_OPTION_PRESERVE_NAMES;
	else
		this->flags &= ~SRPC_JSON_OPTION_PRESERVE_NAMES;
}

inline bool RPCMessage::get_json_preserve_names() const
{
	return this->flags & SRPC_JSON_OPTION_PRESERVE_NAMES;
}

inline void RPCMessage::set_json_print_primitive(bool on)
{
	if (on)
		this->flags |= SRPC_JSON_OPTION_PRINT_PRIMITIVE;
	else
		this->flags &= ~SRPC_JSON_OPTION_PRINT_PRIMITIVE;
}

inline bool RPCMessage::get_json_print_primitive() const
{
	return this->flags & SRPC_JSON_OPTION_PRINT_PRIMITIVE;
}

} // namespace srpc

#endif

