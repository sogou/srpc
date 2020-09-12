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
*/

#ifndef __RPC_THRIFT_BUFFER_H__
#define __RPC_THRIFT_BUFFER_H__

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <arpa/inet.h>
#ifndef htonll
# define htonll(x)   ((((uint64_t)htonl((x) & 0xFFFFFFFF)) << 32) + htonl((x) >> 32))
# define ntohll(x)   ((((uint64_t)ntohl((x) & 0xFFFFFFFF)) << 32) + ntohl((x) >> 32))
#endif

#endif

#include <stdint.h>
#include <string.h>
#include <string>
#include "rpc_thrift_enum.h"
#include "rpc_buffer.h"

namespace srpc
{

static constexpr int32_t THRIFT_VERSION_MASK	=	((int32_t)0xffff0000);
static constexpr int32_t THRIFT_VERSION_1		=	((int32_t)0x80010000);

enum
{
	//THRIFT_PARSE_INIT = 0,
	THRIFT_GET_FRAME_SIZE = 1,
	THRIFT_GET_DATA,
	THRIFT_PARSE_END
};

class ThriftMeta
{
public:
	std::string writebuf;
	std::string method_name;
	int seqid = 0;
	int8_t message_type = TMT_CALL;
	bool is_strict = true;

public:
	ThriftMeta() = default;
	ThriftMeta(const ThriftMeta&) = delete;
	ThriftMeta& operator= (const ThriftMeta&) = delete;
	ThriftMeta(ThriftMeta&& move) = delete;
	ThriftMeta& operator= (ThriftMeta &&move) = delete;
	bool writeI08(int8_t val);
	bool writeI32(int32_t val);
	bool writeString(const std::string& str);
};

class ThriftBuffer
{
public:
	ThriftMeta meta;
	RPCBuffer *buffer;
	size_t readbuf_size = 0;
	size_t framesize_read_byte = 0;
	int32_t framesize = 0;
	int status = THRIFT_GET_FRAME_SIZE;

public:
	ThriftBuffer(RPCBuffer *buf): buffer(buf) { }

	ThriftBuffer(const ThriftBuffer&) = delete;
	ThriftBuffer& operator= (const ThriftBuffer&) = delete;
	ThriftBuffer(ThriftBuffer&& move) = delete;
	ThriftBuffer& operator= (ThriftBuffer &&move) = delete;

public:
	bool readMessageBegin();
	bool readFieldBegin(int8_t& field_type, int16_t& field_id);
	bool readI08(int8_t& val);
	bool readI16(int16_t& val);
	bool readI32(int32_t& val);
	bool readI64(int64_t& val);
	bool readU64(uint64_t& val);
	bool readString(std::string& str);
	bool readStringBody(std::string& str, int32_t slen);
	bool skip(int8_t field_type);

	bool writeMessageBegin();
	bool writeFieldBegin(int8_t field_type, int16_t field_id);
	bool writeFieldStop();
	bool writeI08(int8_t val);
	bool writeI16(int16_t val);
	bool writeI32(int32_t val);
	bool writeI64(int64_t val);
	bool writeU64(uint64_t val);
	bool writeString(const std::string& str);
	bool writeStringBody(const std::string& str);
};

////////
// inl

inline bool ThriftBuffer::readI08(int8_t& val)
{
	return this->buffer->read((char *)&val, 1);
}

inline bool ThriftBuffer::readI16(int16_t& val)
{
	if (!this->buffer->read((char *)&val, 2))
		return false;

	val = ntohs(val);
	return true;
}

inline bool ThriftBuffer::readI32(int32_t& val)
{
	if (!this->buffer->read((char *)&val, 4))
		return false;

	val = ntohl(val);
	return true;
}

inline bool ThriftBuffer::readI64(int64_t& val)
{
	if (!this->buffer->read((char *)&val, 8))
		return false;

	val = ntohll(val);
	return true;
}

inline bool ThriftBuffer::readU64(uint64_t& val)
{
	if (!this->buffer->read((char *)&val, 8))
		return false;

	val = ntohll(val);
	return true;
}

inline bool ThriftBuffer::readFieldBegin(int8_t& field_type, int16_t& field_id)
{
	if (!readI08(field_type))
		return false;

	if (field_type == TDT_STOP)
		field_id = 0;
	else if (!readI16(field_id))
		return false;

	return true;
}

inline bool ThriftBuffer::readString(std::string& str)
{
	int32_t slen;

	if (!readI32(slen) || slen < 0)
		return false;

	if (!readStringBody(str, slen))
		return false;

	return true;
}

inline bool ThriftBuffer::readStringBody(std::string& str, int32_t slen)
{
	if (slen < 0)
		return false;

	str.resize(slen);
	return this->buffer->read(const_cast<char *>(str.c_str()), slen);
}

inline bool ThriftBuffer::writeFieldStop()
{
	return writeI08((int8_t)TDT_STOP);
}

inline bool ThriftBuffer::writeI08(int8_t val)
{
	return this->buffer->write((char *)&val, 1);
}

inline bool ThriftBuffer::writeI16(int16_t val)
{
	int16_t x = htons(val);

	return this->buffer->write((char *)&x, 2);
}

inline bool ThriftBuffer::writeI32(int32_t val)
{
	int32_t x = htonl(val);

	return this->buffer->write((char *)&x, 4);
}

inline bool ThriftBuffer::writeI64(int64_t val)
{
	int64_t x = htonll(val);

	return this->buffer->write((char *)&x, 8);
}

inline bool ThriftBuffer::writeU64(uint64_t val)
{
	uint64_t x = htonll(val);

	return this->buffer->write((char *)&x, 8);
}

inline bool ThriftBuffer::writeFieldBegin(int8_t field_type, int16_t field_id)
{
	if (!writeI08(field_type))
		return false;

	return writeI16(field_id);
}

inline bool ThriftBuffer::writeString(const std::string& str)
{
	int32_t slen = (int32_t)str.size();

	if (!writeI32(slen))
		return false;

	return writeStringBody(str);
}

inline bool ThriftBuffer::writeStringBody(const std::string& str)
{
	return this->buffer->write(str.c_str(), str.size());
}

inline bool ThriftMeta::writeI08(int8_t val)
{
	this->writebuf.append(1, (char)val);
	return true;
}

inline bool ThriftMeta::writeI32(int32_t val)
{
	int32_t x = htonl(val);

	this->writebuf.append((const char *)&x, 4);
	return true;
}

inline bool ThriftMeta::writeString(const std::string& str)
{
	int32_t slen = (int32_t)str.size();

	writeI32(slen);
	if (slen > 0)
		this->writebuf.append(str);

	return true;
}

} // end namespace srpc

#endif

