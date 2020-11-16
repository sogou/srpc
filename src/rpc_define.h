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

#ifndef __SOGOU_RPC_H__
#define __SOGOU_RPC_H__

#if __cplusplus < 201100
#error CPLUSPLUS VERSION required at least C++11. Please use "-std=c++11".
#include <C++11_REQUIRED>
#endif

#include "rpc_server.h"
#include "rpc_client.h"
#include "rpc_span_policies.h"

namespace srpc
{

using SRPCServer = RPCServer<RPCTYPESRPC>;
using SRPCClient = RPCClient<RPCTYPESRPC>;
using SRPCClientTask = SRPCClient::TASK;

using SRPCHttpServer = RPCServer<RPCTYPESRPCHttp>;
using SRPCHttpClient = RPCClient<RPCTYPESRPCHttp>;
using SRPCHttpClientTask = SRPCHttpClient::TASK;

using BRPCServer = RPCServer<RPCTYPEBRPC>;
using BRPCClient = RPCClient<RPCTYPEBRPC>;
using BRPCClientTask = BRPCClient::TASK;

using ThriftServer = RPCServer<RPCTYPEThrift>;
using ThriftClient = RPCClient<RPCTYPEThrift>;
using ThriftClientTask = ThriftClient::TASK;

using ThriftHttpServer = RPCServer<RPCTYPEThriftHttp>;
using ThriftHttpClient = RPCClient<RPCTYPEThriftHttp>;
using ThriftHttpClientTask = ThriftHttpClient::TASK;

} // namespace srpc

#endif

