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

#ifndef __RPC_MESSAGE_H__
#define __RPC_MESSAGE_H__

#include <string.h>
#include <string>
#include <workflow/ProtocolMessage.h>
#include "rpc_basic.h"

namespace sogou
{

static const char *srpc_error_string(int srpc_status_code);

class RPCRequest
{
public:
	virtual bool serialize_meta() = 0;
	virtual bool deserialize_meta() = 0;

	virtual const std::string& get_service_name() const = 0;
	virtual const std::string& get_method_name() const = 0;

	virtual void set_service_name(const std::string& service_name) = 0;
	virtual void set_method_name(const std::string& method_name) = 0;

	virtual void set_seqid(long long seqid) {}
};

class RPCResponse
{
public:
	virtual bool serialize_meta() = 0;
	virtual bool deserialize_meta() = 0;

	virtual int get_status_code() const = 0;
	virtual int get_error() const = 0;
	virtual const char *get_errmsg() const
	{
		return srpc_error_string(get_status_code());
	}

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

static const char *srpc_error_string(int srpc_status_code)
{
	switch (srpc_status_code)
	{
	case RPCStatusOK:
		return "OK";
	case RPCStatusUndefined:
		return "Undefined Error";
	case RPCStatusServiceNotFound:
		return "Service Not Found";
	case RPCStatusMethodNotFound:
		return "Method Not Found";
	case RPCStatusMetaError:
		return "Meta Error";
	case RPCStatusReqCompressSizeInvalid:
		return "Request Compress-size Invalid";
	case RPCStatusReqDecompressSizeInvalid:
		return "Request Decompress-size Invalid";
	case RPCStatusReqCompressNotSupported:
		return "Request Compress Not Supported";
	case RPCStatusReqDecompressNotSupported:
		return "Request Decompress Not Supported";
	case RPCStatusReqCompressError:
		return "Request Compress Error";
	case RPCStatusReqDecompressError:
		return "Request Decompress Error";
	case RPCStatusReqSerializeError:
		return "Request Serialize Error";
	case RPCStatusReqDeserializeError:
		return "Request Deserialize Error";
	case RPCStatusRespCompressSizeInvalid:
		return "Response Compress-size Invalid";
	case RPCStatusRespDecompressSizeInvalid:
		return "Response Decompress-size Invalid";
	case RPCStatusRespCompressNotSupported:
		return "Response Compress Not Supported";
	case RPCStatusRespDecompressNotSupported:
		return "Response Decompress Not Supported";
	case RPCStatusRespCompressError:
		return "Response Compress Error";
	case RPCStatusRespDecompressError:
		return "Response Decompress Error";
	case RPCStatusRespSerializeError:
		return "Response Serialize Error";
	case RPCStatusRespDeserializeError:
		return "Response Deserialize Error";
	case RPCStatusIDLSerializeNotSupported:
		return "IDL Serialize Not Supported";
	case RPCStatusIDLDeserializeNotSupported:
		return "IDL Deserialize Not Supported";
	case RPCStatusURIInvalid:
		return "URI Invalid";
	case RPCStatusUpstreamFailed:
		return "Upstream Failed";
	case RPCStatusSystemError:
		return "System Error. Use get_error() to get errno";
	case RPCStatusSSLError:
		return "SSL Error. Use get_error() to get SSL-Error";
	case RPCStatusDNSError:
		return "DNS Error. Use get_error() to get GAI-Error";
	case RPCStatusProcessTerminated:
		return "Process Terminated";
	default:
		return "Unknown Error";
	}
}

} // namespace sogou

#endif

