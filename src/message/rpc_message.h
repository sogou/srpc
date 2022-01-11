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
};

} // namespace srpc

#endif

