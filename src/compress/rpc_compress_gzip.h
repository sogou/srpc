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

#ifndef __RPC_COMPRESS_GZIP_H__
#define __RPC_COMPRESS_GZIP_H__

//#include <google/protobuf/io/gzip_stream.h>
//#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <zlib.h>
#include "rpc_basic.h"

//#define COMPRESS_LEVEL	6
//#define ZLIB_LEASE_HEADER	100

#define GzipDecompress CommonDecompress
#define ZlibDecompress CommonDecompress

#define GzipDecompressIOVec CommonDecompressIOVec
#define ZlibDecompressIOVec CommonDecompressIOVec

namespace srpc
{

static constexpr int GZIP_LEASE_HEADER		= 20;
static constexpr int WINDOW_BITS			= 15;
static constexpr int OPTION_FORMAT_ZLIB		= 0;
static constexpr int OPTION_FORMAT_GZIP		= 16;
static constexpr int OPTION_FORMAT_AUTO		= 32;

/*
static int protobuf_stream_compress(const ProtobufIDLMessage* msg, char *buf, size_t buflen,
									int compress_type)
{
	google::protobuf::io::GzipOutputStream::Options options;

	switch(compress_type)
	{
	case RPCCompressGzip:
		options.format = google::protobuf::io::GzipOutputStream::GZIP;
		break;
	case RPCCompressZlib:
		options.format = google::protobuf::io::GzipOutputStream::ZLIB;
		break;
	default:
		return -1;
	}

	options.compression_level = COMPRESS_LEVEL;

	google::protobuf::io::ArrayOutputStream arr(buf, buflen);
	google::protobuf::io::GzipOutputStream gzipOutputStream(&arr, options);

	if (!msg->SerializeToZeroCopyStream(&gzipOutputStream))
		return -1;

	gzipOutputStream.Flush();
	gzipOutputStream.Close();

	return arr.ByteCount();
}

static int protobuf_stream_decompress(const char *buf, size_t buflen, ProtobufIDLMessage* msg, size_t msglen,
									  int compress_type)
{
	google::protobuf::io::GzipInputStream::Format format;
	switch(compress_type)
	{
	case RPCCompressGzip:
		format = google::protobuf::io::GzipInputStream::GZIP;
		break;
	case RPCCompressZlib:
		format = google::protobuf::io::GzipInputStream::ZLIB;
		break;
	default:
		return -1;
	}

	google::protobuf::io::ArrayInputStream arr(buf, buflen);
	google::protobuf::io::GzipInputStream gzipInputStream(&arr, format);

	if (!msg->ParseFromZeroCopyStream(&gzipInputStream))
		return -1;

	return msg->ByteSize();
}
*/

static int CommonCompress(const char *msg, size_t msglen,
						  char *buf, size_t buflen, int option_format)
{
	if (!msg)
		return 0;

	z_stream c_stream;

	c_stream.zalloc = (alloc_func)0;
	c_stream.zfree = (free_func)0;
	c_stream.opaque = (voidpf)0;

	if(deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
					WINDOW_BITS | option_format, 8,
					Z_DEFAULT_STRATEGY) != Z_OK)
	{
		return -1;
	}

	c_stream.next_in = (Bytef *)msg;
	c_stream.avail_in = msglen;
	c_stream.next_out = (Bytef *)buf;
	c_stream.avail_out = (uInt)buflen;

	while (c_stream.avail_in != 0 && c_stream.total_in < buflen) 
	{
		if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK)
			return -1;
	}

	if (c_stream.avail_in != 0)
		return c_stream.avail_in;

	for (;;)
	{
		int err = deflate(&c_stream, Z_FINISH);

		if(err == Z_STREAM_END)
			break;

		if(err != Z_OK)
			return -1;
	}

	if (deflateEnd(&c_stream) != Z_OK)
		return -1;

	return c_stream.total_out;
}

/*
 * compress serialized msg into buf.
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static inline int GzipCompress(const char *msg, size_t msglen, char *buf, size_t buflen)
{
	return CommonCompress(msg, msglen, buf, buflen, OPTION_FORMAT_GZIP);
}

/*
 * compress serialized msg into buf.
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static inline int ZlibCompress(const char *msg, size_t msglen, char *buf, size_t buflen)
{
	return CommonCompress(msg, msglen, buf, buflen, OPTION_FORMAT_ZLIB);
}

static constexpr unsigned char dummy_head[2] =
{
	0xB + 0x8 * 0x10,
	(((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
};

/*
 * decompress and parse buf into msg
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int CommonDecompress(const char *buf, size_t buflen, char *msg, size_t msglen)
{
	int err;
	z_stream d_stream = {0}; /* decompression stream */

	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;
	d_stream.next_in = (Bytef *)buf;
	d_stream.avail_in = 0;
	d_stream.next_out = (Bytef *)msg;
	if (inflateInit2(&d_stream, WINDOW_BITS | OPTION_FORMAT_AUTO) != Z_OK)
		return -1;

	while (d_stream.total_out < msglen && d_stream.total_in < buflen)
	{
		d_stream.avail_in = d_stream.avail_out = (uInt)msglen;
		err = inflate(&d_stream, Z_NO_FLUSH);
		if(err == Z_STREAM_END)
			break;

		if (err != Z_OK)
		{
			if (err != Z_DATA_ERROR)
				return -1;

			d_stream.next_in = (Bytef*) dummy_head;
			d_stream.avail_in = sizeof (dummy_head);
			if (inflate(&d_stream, Z_NO_FLUSH) != Z_OK)
				return -1;
		}
	}

	if (inflateEnd(&d_stream) != Z_OK)
		return -1;

	return d_stream.total_out;
}

static int CommonCompressIOVec(RPCBuffer *src, RPCBuffer *dst, int option_format)
{
	z_stream c_stream;
	int err;
	size_t total_alloc = 0;
	const void *in;
	void *out;
	size_t buflen = src->size();
	size_t out_len = buflen;

	c_stream.zalloc = (alloc_func)0;
	c_stream.zfree = (free_func)0;
	c_stream.opaque = (voidpf)0;

	if (deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
					 WINDOW_BITS | option_format, 8,
					 Z_DEFAULT_STRATEGY) != Z_OK)
	{
		return -1;
	}

	c_stream.avail_in = 0;
	c_stream.avail_out = 0;

	while (c_stream.total_in < buflen)
	{
		if (c_stream.avail_in == 0)
		{
			if ((c_stream.avail_in = (uInt)src->fetch(&in)) == 0)
				return -1;

			c_stream.next_in = static_cast<Bytef *>(const_cast<void *>(in));
		}

		if (c_stream.avail_out == 0)
		{
			if (dst->acquire(&out, &out_len) == false)
				return -1;

			total_alloc += out_len;
			c_stream.next_out = static_cast<Bytef *>(out);
			c_stream.avail_out = (uInt)out_len;
		}

		if (deflate(&c_stream, Z_NO_FLUSH) != Z_OK)
			return -1;
	}

//	if (c_stream.avail_in != 0)
//		return c_stream.avail_in;

	for (;;)
	{
		if (c_stream.avail_out == 0)
		{
			if (dst->acquire(&out, &out_len) == false)
				return -1;

			total_alloc += out_len;
			c_stream.next_out  = static_cast<Bytef *>(out);
			c_stream.avail_out = (uInt)out_len;
		}

		err = deflate(&c_stream, Z_FINISH);
		if(err == Z_STREAM_END)
			break;

		if(err != Z_OK)
			return -1;
	}

	if (deflateEnd(&c_stream) != Z_OK)
		return -1;

	dst->backup(total_alloc - c_stream.total_out);
	return c_stream.total_out;
}

/*
 * compress serialized RPCBuffer src(Source) into RPCBuffer dst(Sink).
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int GzipCompressIOVec(RPCBuffer *src, RPCBuffer *dst)
{
	return CommonCompressIOVec(src, dst, OPTION_FORMAT_GZIP);	
}

/*
 * compress serialized RPCBuffer src(Source) into RPCBuffer dst(Sink).
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int ZlibCompressIOVec(RPCBuffer *src, RPCBuffer *dst)
{
	return CommonCompressIOVec(src, dst, OPTION_FORMAT_ZLIB);	
}

/*
 * decompress RPCBuffer src(Source) into RPCBuffer dst(Sink).
 * ret: -1: failed
 * 		>0: byte count of compressed data
 */
static int CommonDecompressIOVec(RPCBuffer *src, RPCBuffer *dst)
{
	int err;
	z_stream d_stream = { 0 }; /* decompression stream */
	size_t total_alloc = 0;
	const void *in;
	void *out;
	size_t buflen = src->size();
	size_t out_len = buflen;

	d_stream.zalloc = (alloc_func)0;
	d_stream.zfree = (free_func)0;
	d_stream.opaque = (voidpf)0;

	if (inflateInit2(&d_stream, WINDOW_BITS | OPTION_FORMAT_AUTO) != Z_OK)
		return -1;

	d_stream.avail_in = 0;
	d_stream.avail_out = 0;

	while (d_stream.total_in < buflen)
	{
		if (d_stream.avail_in == 0)
		{
			if ((d_stream.avail_in = (uInt)src->fetch(&in)) == 0)
				return -1;

			d_stream.next_in = static_cast<Bytef *>(const_cast<void *>(in));
		}

		if (d_stream.avail_out == 0)
		{
			if (dst->acquire(&out, &out_len) == false)
				return -1;

			total_alloc += out_len;
			d_stream.next_out = static_cast<Bytef *>(out);
			d_stream.avail_out = (uInt)out_len;
		}

		err = inflate(&d_stream, Z_NO_FLUSH);
		if (err == Z_STREAM_END)
			break;

		if (err != Z_OK)
		{
			if (err != Z_DATA_ERROR)
				return -1;

			d_stream.next_in = (Bytef*) dummy_head;
			d_stream.avail_in = sizeof (dummy_head);
			if (inflate(&d_stream, Z_NO_FLUSH) != Z_OK)
				return -1;
		}
	}

	if (inflateEnd(&d_stream) != Z_OK)
		return -1;

	dst->backup(total_alloc - d_stream.total_out);
	return d_stream.total_out;
}

/*
 * lease size after compress origin_size data
 */
static int GzipLeaseSize(size_t origin_size)
{
	return (int)(origin_size + 5 * (origin_size / 16383 + 1) + GZIP_LEASE_HEADER);
}

static int ZlibLeaseSize(size_t origin_size)
{
	return compressBound((uLong)origin_size);
}

} // end namespace srpc

#endif

