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

#include "snappy.h"
#include "snappy-sinksource.h"
#include "rpc_compress_snappy.h"

namespace srpc
{

class RPCSnappySink : public snappy::Sink
{
public:
	RPCSnappySink(RPCBuffer *buf)
	{
		this->buf = buf;
	}

	void Append(const char *bytes, size_t n) override
	{
		this->buf->append(bytes, n, BUFFER_MODE_COPY);
	}

	size_t size() const
	{
		return this->buf->size();
	}

private:
	RPCBuffer *buf;
};

class RPCSnappySource : public snappy::Source
{
public:
	RPCSnappySource(RPCBuffer *buf)
	{
		this->buf = buf;
		this->buf_size = this->buf->size();
		this->pos = 0;
	}

	size_t Available() const override
	{
		return this->buf_size - this->pos;
	}

	const char *Peek(size_t *len) override
	{
		const void *pos;

		*len = this->buf->peek(&pos);
		return (const char *)pos;
	}

	void Skip(size_t n) override
	{
		this->pos += this->buf->seek(n);
	}

private:
	RPCBuffer *buf;
	size_t buf_size;
	size_t pos;
};

int SnappyManager::SnappyCompress(const char *msg, size_t msglen,
								  char *buf, size_t buflen)
{
	size_t compressed_len = buflen;

	snappy::RawCompress(msg, msglen, buf, &compressed_len);
	if (compressed_len > buflen)
		return -1;

	return (int)compressed_len;
}

int SnappyManager::SnappyDecompress(const char *buf, size_t buflen,
									char *msg, size_t msglen)
{
	size_t origin_len;

	if (snappy::GetUncompressedLength(buf, buflen, &origin_len) &&
		origin_len <= msglen &&
		snappy::RawUncompress(buf, buflen, msg))
	{
		return (int)origin_len;
	}

	return -1;
}

int SnappyManager::SnappyCompressIOVec(RPCBuffer *src, RPCBuffer *dst)
{
	RPCSnappySource source(src);
	RPCSnappySink sink(dst);

	return (int)snappy::Compress(&source, &sink);
}

int SnappyManager::SnappyDecompressIOVec(RPCBuffer *src, RPCBuffer *dst)
{
	RPCSnappySource source(src);
	RPCSnappySink sink(dst);

//	if (snappy::IsValidCompressed(&source))
//	if (snappy::GetUncompressedLength(&source, &origin_len))
	return (int)snappy::Uncompress(&source, &sink) ? sink.size() : -1;
}

int SnappyManager::SnappyLeaseSize(size_t origin_size)
{
	return (int)snappy::MaxCompressedLength(origin_size);
}

} // end namespace srpc
