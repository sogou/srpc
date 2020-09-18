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

#ifndef __RPC_COMPRESS_H__
#define __RPC_COMPRESS_H__

#include "rpc_basic.h"
#include "rpc_options.h"

namespace srpc
{

using CompressFunction = int (*)(const char*, size_t, char *, size_t);
using DecompressFunction = int (*)(const char *, size_t, char *, size_t);
using CompressIOVecFunction = int (*)(RPCBuffer *, RPCBuffer *);
using DempressIOVecFunction = int (*)(RPCBuffer *, RPCBuffer *);
using LeaseSizeFunction = int (*)(size_t);

class CompressHandler
{
public:
	CompressHandler()
	{
		//this->type = RPCCompressNone;
		this->compress = nullptr;
		this->decompress = nullptr;
		this->compress_iovec = nullptr;
		this->decompress_iovec = nullptr;
		this->lease_size = nullptr;
	}

	//int type;
	CompressFunction compress;
	DecompressFunction decompress;
	CompressIOVecFunction compress_iovec;
	DempressIOVecFunction decompress_iovec;
	LeaseSizeFunction lease_size;	
};

class RPCCompressor
{
public:
	static RPCCompressor *get_instance()
	{
		static RPCCompressor kInstance;
		return &kInstance;
	}

public:
	// parse message from compressed data
	// 		-1: error
	// 		-2, invalid compress type or decompress function
	int parse_from_compressed(const char *buf, size_t buflen, char *msg, size_t msglen, int type) const;
	int parse_from_compressed(RPCBuffer *src, RPCBuffer *dest, int type) const;

	// serialized to compressed data
	// 		-1: error
	// 		-2, invalid compress type or compress function
	int serialize_to_compressed(const char *msg, size_t msglen, char *buf, size_t buflen, int type) const;
	int serialize_to_compressed(RPCBuffer *src, RPCBuffer *dest, int type) const;

	/*
	 * ret: >0: the theoretically lease size of compressed data
	 * 		-1: error
	 * 		-2, invalid compress type or lease_size function
	 */
	int lease_compressed_size(int type, size_t origin_size) const;

	/*
	 * Add existed compress_type
	 * ret:  0, success
	 * 		 1, handler existed and update success
	 * 		-2, invalid compress type
	 */
	int add(RPCCompressType type);

	/*
	 * Add self-define compress algorithm
	 * ret:  0, success
	 * 		 1, handler existed and update success
	 * 		-2, invalid compress type or compress/decompress/lease_size function
	 */
	int add_handler(int type, CompressHandler&& handler);

	const CompressHandler *find_handler(int type) const;

	// clear all the registed handler
	void clear();

private:
	RPCCompressor()
	{
		this->add(RPCCompressGzip);
		this->add(RPCCompressZlib);
		this->add(RPCCompressSnappy);
		this->add(RPCCompressLz4);
	}

	CompressHandler handler[SRPC_COMPRESS_TYPE_MAX];
};

////////
// inl

inline int RPCCompressor::add_handler(int type, CompressHandler&& handler)
{
	if (type >= SRPC_COMPRESS_TYPE_MAX || type <= RPCCompressNone)
		return -2;

	if (!handler.compress || !handler.decompress || !handler.lease_size)
		return -2;

	int ret = 0;

	if (this->handler[type].compress
		&& this->handler[type].decompress
		&& this->handler[type].lease_size)
	{
		ret = 1;
	}

	this->handler[type] = std::move(handler);
	return ret;
}

inline const CompressHandler *RPCCompressor::find_handler(int type) const
{
	if (type >= SRPC_COMPRESS_TYPE_MAX || type <= RPCCompressNone)
	{
		return NULL;
	}

	if (!this->handler[type].compress
		|| !this->handler[type].decompress
		|| !this->handler[type].lease_size)
	{
		return NULL;
	}

	return &this->handler[type];
}

inline int RPCCompressor::parse_from_compressed(const char *buf, size_t buflen,
												char *msg, size_t msglen,
												int type) const
{
	if (type >= SRPC_COMPRESS_TYPE_MAX
		|| type <= RPCCompressNone
		|| !this->handler[type].decompress)
	{
		return -2;
	}

	return this->handler[type].decompress(buf, buflen, msg, msglen);
}

inline int RPCCompressor::parse_from_compressed(RPCBuffer *src, RPCBuffer *dest,
												int type) const
{
	if (type >= SRPC_COMPRESS_TYPE_MAX
		|| type <= RPCCompressNone
		|| !this->handler[type].decompress_iovec)
	{
		return -2;
	}

	return this->handler[type].decompress_iovec(src, dest);
}

inline int RPCCompressor::serialize_to_compressed(const char *msg, size_t msglen,
												  char *buf, size_t buflen,
												  int type) const
{
	if (type >= SRPC_COMPRESS_TYPE_MAX
		|| type <= RPCCompressNone
		|| !this->handler[type].compress)
	{
		return -2;
	}

	return this->handler[type].compress(msg, msglen, buf, buflen);
}

inline int RPCCompressor::serialize_to_compressed(RPCBuffer *src, RPCBuffer *dest,
												  int type) const
{
	if (type >= SRPC_COMPRESS_TYPE_MAX
		|| type <= RPCCompressNone
		|| !this->handler[type].compress)
	{
		return -2;
	}

	return this->handler[type].compress_iovec(src, dest);
}

inline int RPCCompressor::lease_compressed_size(int type, size_t origin_size) const
{
	if (type >= SRPC_COMPRESS_TYPE_MAX
		|| type <= RPCCompressNone
		|| !this->handler[type].lease_size)
	{
		return -2;
	}

	return this->handler[type].lease_size(origin_size);
}

inline void RPCCompressor::clear()
{
	for (int i = 0; i < SRPC_COMPRESS_TYPE_MAX; i++)
	{
		//this->handler[i].type = RPCCompressNone;
		this->handler[i].compress = nullptr;
		this->handler[i].decompress = nullptr;
		this->handler[i].lease_size = nullptr;
	}
}

} // namespace srpc

#endif

