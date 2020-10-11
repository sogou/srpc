/*
  Copyright (c) 2020 sogou, Inc.

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

#ifndef __RPC_ZERO_COPY_STREAM_H__
#define __RPC_ZERO_COPY_STREAM_H__

#include <google/protobuf/io/zero_copy_stream.h>

namespace srpc
{

class RPCOutputStream : public google::protobuf::io::ZeroCopyOutputStream
{
public:
	RPCOutputStream(RPCBuffer *buf, size_t size);
	bool Next(void **data, int *size) override;
	void BackUp(int count) override;
	int64_t ByteCount() const override;

private:
	RPCBuffer *buf;
	size_t size;
};

class RPCInputStream : public google::protobuf::io::ZeroCopyInputStream
{
public:
	RPCInputStream(RPCBuffer *buf);
	bool Next(const void **data, int *size) override;
	void BackUp(int count) override;
	bool Skip(int count) override;
	int64_t ByteCount() const override;

private:
	RPCBuffer *buf;
};

inline RPCOutputStream::RPCOutputStream(RPCBuffer *buf, size_t size)
{
	this->buf = buf;
	this->size = size;
}

inline bool RPCOutputStream::Next(void **data, int *size)
{
	size_t tmp;

	if (this->size > 0)
	{
		tmp = this->size;
		if (this->buf->acquire(data, &tmp))
		{
			this->size -= tmp;
			*size = (int)tmp;
			return true;
		}
	}
	else
	{
		tmp = this->buf->acquire(data);
		if (tmp > 0)
		{
			*size = (int)tmp;
			return true;
		}
	}

	return false;
}

inline void RPCOutputStream::BackUp(int count)
{
	this->buf->backup(count);
}

inline int64_t RPCOutputStream::ByteCount() const
{
	return this->buf->size();
}

inline RPCInputStream::RPCInputStream(RPCBuffer *buf)
{
	this->buf = buf;
}

inline bool RPCInputStream::Next(const void **data, int *size)
{
	size_t len = this->buf->fetch(data);

	if (len == 0)
		return false;

	*size = (int)len;
	return true;
}

inline bool RPCInputStream::Skip(int count)
{
	return this->buf->seek(count) == count ? true : false;
}

inline void RPCInputStream::BackUp(int count)
{
	this->buf->seek(0 - count);
}

inline int64_t RPCInputStream::ByteCount() const
{
	return (int64_t)this->buf->size();
}

} // namespace srpc

#endif

