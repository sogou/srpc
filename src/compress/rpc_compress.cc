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

#include "rpc_compress.h"
#include "rpc_compress_gzip.h"
#include "rpc_compress_snappy.h"
#include "rpc_compress_lz4.h"

namespace srpc
{

int RPCCompressor::add(RPCCompressType type)
{
	if (type >= SRPC_COMPRESS_TYPE_MAX || type <= RPCCompressNone)
		return -2;

	int ret = 0;

	if (this->handler[type].compress
		&& this->handler[type].decompress
		&& this->handler[type].lease_size)
	{
		ret = 1;
	}

	//this->handler[type].type = type;

	switch (type)
	{
	case RPCCompressSnappy:
		this->handler[type].compress = SnappyManager::SnappyCompress;
		this->handler[type].decompress = SnappyManager::SnappyDecompress;
		this->handler[type].compress_iovec = SnappyManager::SnappyCompressIOVec;
		this->handler[type].decompress_iovec = SnappyManager::SnappyDecompressIOVec;
		this->handler[type].lease_size = SnappyManager::SnappyLeaseSize;
		break;
	case RPCCompressGzip:
		this->handler[type].compress = GzipCompress;
		this->handler[type].decompress = GzipDecompress;
		this->handler[type].compress_iovec = GzipCompressIOVec;
		this->handler[type].decompress_iovec = GzipDecompressIOVec;
		this->handler[type].lease_size = GzipLeaseSize;
		break;
	case RPCCompressZlib:
		this->handler[type].compress = ZlibCompress;
		this->handler[type].decompress = ZlibDecompress;
		this->handler[type].compress_iovec = ZlibCompressIOVec;
		this->handler[type].decompress_iovec = ZlibDecompressIOVec;
		this->handler[type].lease_size = ZlibLeaseSize;
		break;
	case RPCCompressLz4:
		this->handler[type].compress = LZ4Compress;
		this->handler[type].decompress = LZ4Decompress;
		this->handler[type].compress_iovec = LZ4CompressIOVec;
		this->handler[type].decompress_iovec = LZ4DecompressIOVec;
		this->handler[type].lease_size = LZ4LeaseSize;
		break;
	default:
		ret = -2;
		break;
	}

	return ret;
}

} // namespace srpc

