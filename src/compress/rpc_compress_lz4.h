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

#ifndef __RPC_COMPRESS_LZ4_H__
#define __RPC_COMPRESS_LZ4_H__

#include "lz4.h"
#include "lz4frame.h"
#include "rpc_basic.h"

namespace srpc
{

//#define IN_CHUNK_SIZE  (16*1024)

static constexpr LZ4F_preferences_t kPrefs = {
	{
		LZ4F_max256KB,
		LZ4F_blockLinked,
		LZ4F_noContentChecksum,
		LZ4F_frame,
		0 /* unknown content size */,
		0 /* no dictID */ ,
		LZ4F_noBlockChecksum
	},
	0,   /* compression level; 0 == default */
	0,   /* autoflush */
	0,   /* favor decompression speed */
	{ 0, 0, 0 },  /* reserved, must be set to 0 */
};

/*
static size_t get_block_size(const LZ4F_frameInfo_t* info)
{
	switch (info->blockSizeID)
	{
		case LZ4F_default:
		case LZ4F_max64KB:  return 1 << 16;
		case LZ4F_max256KB: return 1 << 18;
		case LZ4F_max1MB:   return 1 << 20;
		case LZ4F_max4MB:   return 1 << 22;
		default:
			printf("Impossible with expected frame specification (<=v1.6.1)\n");
	}
	return 0;
}
*/

/*
 * compress serialized msg into buf.
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int LZ4Compress(const char* msg, size_t msglen, char *buf, size_t buflen)
{
	int ret = LZ4_compress_default(msg, buf, (int)msglen, (int)buflen);

	if (ret < 0)
		ret = -1;

	return ret;
}

/*
 * decompress and parse buf into msg
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int LZ4Decompress(const char *buf, size_t buflen, char *msg, size_t msglen)
{
	int ret = LZ4_decompress_safe(buf, msg, (int)buflen, (int)msglen);

	if (ret < 0)
		return -1;

	return ret;
}

/*
 * compress RPCBuffer src into RPCBuffer dst
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int LZ4CompressIOVec(RPCBuffer *src, RPCBuffer *dst)
{
	const void *in_buf;
	size_t in_len;
	void *out_buf;
	size_t out_len;
	size_t total_out;
	size_t header_size;
	size_t compressed_size;
	LZ4F_cctx *ctx = NULL;
	size_t const ctx_status = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);

	if (LZ4F_isError(ctx_status))
	{
		LZ4F_freeCompressionContext(ctx);
		return -1;
	}

	out_len = LZ4F_HEADER_SIZE_MAX;
	if (dst->acquire(&out_buf, &out_len) == false)
	{
		LZ4F_freeCompressionContext(ctx);
		return -1;
	}

	// write frame header
	header_size = LZ4F_compressBegin(ctx, out_buf, out_len, &kPrefs);
	if (LZ4F_isError(header_size))
	{
		LZ4F_freeCompressionContext(ctx);
		return -1;
	}

	dst->backup(out_len - header_size);
	total_out = header_size;
	//int count = 0;
	// write every in_buf
	while ((in_len = src->fetch(&in_buf)) != 0)
	{
		//count++;
		out_len = LZ4F_compressBound(in_len, &kPrefs);
		if (dst->acquire(&out_buf, &out_len) == false)
		{
			LZ4F_freeCompressionContext(ctx);
			return -1;
		}

		compressed_size = LZ4F_compressUpdate(ctx, out_buf, out_len,
												   in_buf, in_len, NULL);
		if (LZ4F_isError(compressed_size))
		{
			LZ4F_freeCompressionContext(ctx);
			return -1;
		}

		dst->backup(out_len - compressed_size);
		total_out += compressed_size;
	}

	// flush whatever remains within internal buffers
	out_len = LZ4F_compressBound(0, &kPrefs);
	if (dst->acquire(&out_buf, &out_len) == false)
	{
		LZ4F_freeCompressionContext(ctx);
		return -1;
	}

	compressed_size = LZ4F_compressEnd(ctx, out_buf, out_len, NULL);
	if (LZ4F_isError(compressed_size))
	{
		LZ4F_freeCompressionContext(ctx);
		return -1;
	}

	dst->backup(out_len - compressed_size);
	total_out += compressed_size;
	LZ4F_freeCompressionContext(ctx);
	return (int)total_out;
}

/*
 * decompress RPCBuffer src into RPCBuffer dst
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int LZ4DecompressIOVec(RPCBuffer *src, RPCBuffer *dst)
{
	const void *in_buf;
	size_t in_len;
	void *out_buf;
	size_t out_len;
	size_t consumed_len;
	size_t decompressed_size;
	size_t total_out = 0;
	LZ4F_dctx *dctx = NULL;
	size_t const dctx_status = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);

	if (LZ4F_isError(dctx_status))
	{
		LZ4F_freeDecompressionContext(dctx);
		return -1;
	}

	// get framed info
	LZ4F_frameInfo_t info;
	memset(&info, 0, sizeof (info));
	in_len = src->fetch(&in_buf);
	consumed_len = in_len;

	size_t const frame_info_ret = LZ4F_getFrameInfo(dctx, &info, in_buf,
														 &consumed_len);

	if (LZ4F_isError(frame_info_ret))
	{
		LZ4F_freeDecompressionContext(dctx);
		return -1;
	}

/*
	size_t const dest_capacity = get_block_size(&info);
	if (dest_capacity == 0)
	{
		LZ4F_freeDecompressionContext(ctx);
		return -1;
	}
	fprintf(stderr, "get_block_size=%d\n", dest_capacity);
*/
	size_t ret = 1;
	bool first_chunk = true;
	const char *start;
	const char *end;

	while (ret != 0)
	{
		if (first_chunk)
		{
			in_len = in_len - consumed_len;
			in_buf = (const char *)in_buf + consumed_len;
			first_chunk = false;
		} else {
			in_len = src->fetch(&in_buf);
		}

		out_len = in_len;
		start = (const char *)in_buf;
		end = (const char *)in_buf + in_len;
		consumed_len = 0;
		while (start != end && ret != 0)
		{
			if (dst->acquire(&out_buf, &out_len) == false)
			{
				LZ4F_freeDecompressionContext(dctx);
				return -1;
			}

 			decompressed_size = out_len;
			consumed_len = end - start;

			ret = LZ4F_decompress(dctx, out_buf, &decompressed_size,
									   start, &consumed_len, NULL);
			if (LZ4F_isError(ret))
			{
				LZ4F_freeDecompressionContext(dctx);
				return -1;
			}

			start = start + consumed_len;

			dst->backup(out_len - decompressed_size);		
			total_out += decompressed_size;
		}

		if (start != end)
		{
			LZ4F_freeDecompressionContext(dctx);
			return -1;
		}
	}

	LZ4F_freeDecompressionContext(dctx);
	return (int)total_out;
}

/*
 * lease size after compress origin_size data
 */
static int LZ4LeaseSize(size_t origin_size)
{
	return LZ4_compressBound((int)origin_size);
}

} // end namespace srpc

#endif

