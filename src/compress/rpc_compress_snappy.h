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

#ifndef __RPC_COMPRESS_SNAPPY_H__
#define __RPC_COMPRESS_SNAPPY_H__

#include "rpc_basic.h"

namespace srpc
{

class SnappyManager
{
public:
	/*
	 * compress serialized msg into buf.
	 * ret: -1: failed
	 * 		>0: byte count of compressed data
	 */
	static int SnappyCompress(const char *msg, size_t msglen, char *buf, size_t buflen);

	/*
	 * decompress and parse buf into msg
	 * ret: -1: failed
	 * 		>0: byte count of compressed data
	 */
	static int SnappyDecompress(const char *buf, size_t buflen, char *msg, size_t msglen);

	/*
	 * compress RPCBuffer src(Source) into RPCBuffer dst(Sink)
	 * ret: -1: failed
	 * 		>0: byte count of compressed data
	 */
	static int SnappyCompressIOVec(RPCBuffer *src, RPCBuffer *dst);

	/*
	 * decompress RPCBuffer src(Source) into RPCBuffer dst(Sink)
	 * ret: -1: failed
	 * 		>0: byte count of compressed data
	 */
	static int SnappyDecompressIOVec(RPCBuffer *src, RPCBuffer *dst);

	/*
	 * lease size after compress origin_size data
	 */
	static int SnappyLeaseSize(size_t origin_size);
};

} // end namespace srpc

#endif

