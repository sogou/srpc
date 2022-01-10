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

#ifndef __RPC_THRIFT_BUFFER_H__
#define __RPC_THRIFT_BUFFER_H__

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

} // end namespace srpc

#endif

