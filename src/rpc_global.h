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

  Authors: Wu Jiaxu (wujiaxu@sogou-inc.com)
           Li Yingxin (liyingxin@sogou-inc.com)
*/

#ifndef __RPC_GLOBAL_H__
#define __RPC_GLOBAL_H__

#include <workflow/URIParser.h>
#include "rpc_options.h"

namespace sogou
{

class SRPCGlobal
{
public:
	static SRPCGlobal *get_instance()
	{
		static SRPCGlobal kInstance;

		return &kInstance;
	}

public:
	const char *get_srpc_version() const;

	bool task_init(RPCClientParams& params, ParsedURI& uri,
				   struct sockaddr_storage *ss, socklen_t *ss_len) const;

private:
	SRPCGlobal();
};

} // namespace sogou

#endif

