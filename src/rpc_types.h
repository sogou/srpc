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

#ifndef __RPC_TYPE_H__
#define __RPC_TYPE_H__

#include "rpc_basic.h"
#include "rpc_message.h"
#include "rpc_message_srpc.h"
#include "rpc_message_thrift.h"
#include "rpc_message_brpc.h"
#include "rpc_message_trpc.h"

namespace srpc
{

struct RPCTYPESRPC
{
	using REQ = SRPCStdRequest;
	using RESP = SRPCStdResponse;
	static constexpr RPCDataType default_data_type = RPCDataProtobuf;

	static inline void server_reply_init(const REQ *req, RESP *resp)
	{
		resp->set_data_type(req->get_data_type());
	}
};

struct RPCTYPESRPCHttp
{
	using REQ = SRPCHttpRequest;
	using RESP = SRPCHttpResponse;
	static constexpr RPCDataType default_data_type = RPCDataJson;

	static inline void server_reply_init(const REQ *req, RESP *resp)
	{
		resp->set_data_type(req->get_data_type());
	}
};

struct RPCTYPEGRPC
{
	//using REQ = GRPCHttp2Request;
	//using RESP = GRPCHttp2Response;
	static constexpr RPCDataType default_data_type = RPCDataProtobuf;
};

struct RPCTYPEBRPC
{
	using REQ = BRPCStdRequest;
	using RESP = BRPCStdResponse;
	static constexpr RPCDataType default_data_type = RPCDataProtobuf;

	static inline void server_reply_init(const REQ *req, RESP *resp)
	{
		resp->set_correlation_id(req->get_correlation_id());
	}
};

struct RPCTYPEThrift
{
	using REQ = ThriftStdRequest;
	using RESP = ThriftStdResponse;
	static constexpr RPCDataType default_data_type = RPCDataThrift;

	static inline void server_reply_init(const REQ *req, RESP *resp)
	{
		resp->set_data_type(req->get_data_type());
		auto *req_meta = req->get_meta();
		auto *resp_meta = resp->get_meta();

		resp_meta->is_strict = req_meta->is_strict;
		resp_meta->seqid = req_meta->seqid;
		resp_meta->method_name = req_meta->method_name;
	}
};

struct RPCTYPEThriftHttp
{
	using REQ = ThriftHttpRequest;
	using RESP = ThriftHttpResponse;
	static constexpr RPCDataType default_data_type = RPCDataThrift;

	static inline void server_reply_init(const REQ *req, RESP *resp)
	{
		resp->set_data_type(req->get_data_type());
		auto *req_meta = req->get_meta();
		auto *resp_meta = resp->get_meta();

		resp_meta->is_strict = req_meta->is_strict;
		resp_meta->seqid = req_meta->seqid;
		resp_meta->method_name = req_meta->method_name;
	}
};

struct RPCTYPETRPC
{
	using REQ = TRPCStdRequest;
	using RESP = TRPCStdResponse;
	static constexpr RPCDataType default_data_type = RPCDataProtobuf;

	static inline void server_reply_init(const REQ *req, RESP *resp)
	{
		const_cast<REQ *>(req)->trim_method_prefix();
		resp->set_request_id(req->get_request_id());
	}
};

struct RPCTYPETRPCHttp
{
	using REQ = TRPCHttpRequest;
	using RESP = TRPCHttpResponse;
	static constexpr RPCDataType default_data_type = RPCDataJson;

	static inline void server_reply_init(const REQ *req, RESP *resp)
	{
		resp->set_data_type(req->get_data_type());
		const_cast<REQ *>(req)->trim_method_prefix();
		resp->set_request_id(req->get_request_id());
	}
};

} // end namespace srpc

#endif

