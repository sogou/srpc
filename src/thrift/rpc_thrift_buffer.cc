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

#include "rpc_thrift_buffer.h"
#include "rpc_basic.h"

namespace srpc
{

bool ThriftBuffer::readI08(int8_t& val)
{
	return this->buffer->read((char *)&val, 1);
}

bool ThriftBuffer::readI16(int16_t& val)
{
	if (!this->buffer->read((char *)&val, 2))
		return false;

	val = ntohs(val);
	return true;
}

bool ThriftBuffer::readI32(int32_t& val)
{
	if (!this->buffer->read((char *)&val, 4))
		return false;

	val = ntohl(val);
	return true;
}

bool ThriftBuffer::readI64(int64_t& val)
{
	if (!this->buffer->read((char *)&val, 8))
		return false;

	val = ntohll(val);
	return true;
}

bool ThriftBuffer::readU64(uint64_t& val)
{
	if (!this->buffer->read((char *)&val, 8))
		return false;

	val = ntohll(val);
	return true;
}

bool ThriftBuffer::readFieldBegin(int8_t& field_type, int16_t& field_id)
{
	if (!readI08(field_type))
		return false;

	if (field_type == TDT_STOP)
		field_id = 0;
	else if (!readI16(field_id))
		return false;

	return true;
}

bool ThriftBuffer::readString(std::string& str)
{
	int32_t slen;

	if (!readI32(slen) || slen < 0)
		return false;

	if (!readStringBody(str, slen))
		return false;

	return true;
}

bool ThriftBuffer::readStringBody(std::string& str, int32_t slen)
{
	if (slen < 0)
		return false;

	str.resize(slen);
	return this->buffer->read(const_cast<char *>(str.c_str()), slen);
}

bool ThriftBuffer::writeFieldStop()
{
	return writeI08((int8_t)TDT_STOP);
}

bool ThriftBuffer::writeI08(int8_t val)
{
	return this->buffer->write((char *)&val, 1);
}

bool ThriftBuffer::writeI16(int16_t val)
{
	int16_t x = htons(val);

	return this->buffer->write((char *)&x, 2);
}

bool ThriftBuffer::writeI32(int32_t val)
{
	int32_t x = htonl(val);

	return this->buffer->write((char *)&x, 4);
}

bool ThriftBuffer::writeI64(int64_t val)
{
	int64_t x = htonll(val);

	return this->buffer->write((char *)&x, 8);
}

bool ThriftBuffer::writeU64(uint64_t val)
{
	uint64_t x = htonll(val);

	return this->buffer->write((char *)&x, 8);
}

bool ThriftBuffer::writeFieldBegin(int8_t field_type, int16_t field_id)
{
	if (!writeI08(field_type))
		return false;

	return writeI16(field_id);
}

bool ThriftBuffer::writeString(const std::string& str)
{
	int32_t slen = (int32_t)str.size();

	if (!writeI32(slen))
		return false;

	return writeStringBody(str);
}

bool ThriftBuffer::writeStringBody(const std::string& str)
{
	return this->buffer->write(str.c_str(), str.size());
}

bool ThriftMeta::writeI08(int8_t val)
{
	this->writebuf.append(1, (char)val);
	return true;
}

bool ThriftMeta::writeI32(int32_t val)
{
	int32_t x = htonl(val);

	this->writebuf.append((const char *)&x, 4);
	return true;
}

bool ThriftMeta::writeString(const std::string& str)
{
	int32_t slen = (int32_t)str.size();

	writeI32(slen);
	if (slen > 0)
		this->writebuf.append(str);

	return true;
}
bool ThriftBuffer::readMessageBegin()
{
	int32_t header;

	if (!readI32(header))
		return false;

	if (header < 0)
	{
		int32_t version = header & THRIFT_VERSION_MASK;
		if (version != THRIFT_VERSION_1)
			return false;//bad version

		if (!readString(meta.method_name))
			return false;

		if (!readI32(meta.seqid))
			return false;

		meta.message_type = header & 0xFF;
		meta.is_strict = true;
	}
	else
	{
		if (!readStringBody(meta.method_name, header))
			return false;

		if (!readI08(meta.message_type))
			return false;

		if (!readI32(meta.seqid))
			return false;

		meta.is_strict = false;
	}

	return true;
}

bool ThriftBuffer::writeMessageBegin()
{
	if (meta.is_strict)
	{
		int32_t version = (THRIFT_VERSION_1) | ((int32_t)meta.message_type);

		if (!meta.writeI32(version))
			return false;

		if (!meta.writeString(meta.method_name))
			return false;

		if (!meta.writeI32(meta.seqid))
			return false;
	}
	else
	{
		if (!meta.writeString(meta.method_name))
			return false;

		if (!meta.writeI08(meta.message_type))
			return false;

		if (!meta.writeI32(meta.seqid))
			return false;
	}

	return true;
}

bool ThriftBuffer::skip(int8_t field_type)
{
	switch (field_type)
	{
	case TDT_I08:
	case TDT_BOOL:
		return this->buffer->seek(1) == 1;

	case TDT_I16:
		return this->buffer->seek(2) == 2;

	case TDT_I32:
		return this->buffer->seek(4) == 4;

	case TDT_I64:
	case TDT_U64:
	case TDT_DOUBLE:
		return this->buffer->seek(8) == 8;

	case TDT_STRING:
	case TDT_UTF8:
	case TDT_UTF16:
	{
		int32_t slen;

		if (!readI32(slen) || slen < 0)
			return false;

		return this->buffer->seek(slen) == slen;
	}
	case TDT_STRUCT:
	{
		int8_t field_type;
		int16_t field_id;

		while (true)
		{
			if (!readFieldBegin(field_type, field_id))
				return false;

			if (field_type == TDT_STOP)
				break;

			if (!skip(field_type))
				return false;
		}

		break;
	}
	case TDT_MAP:
	{
		int8_t key_type;
		int8_t val_type;
		int32_t count;

		if (!readI08(key_type))
			return false;

		if (!readI08(val_type))
			return false;

		if (!readI32(count))
			return false;

		for (int32_t i = 0; i < count; i++)
		{
			if (!skip(key_type))
				return false;

			if (!skip(val_type))
				return false;
		}

		break;
	}
	case TDT_LIST:
	case TDT_SET:
	{
		int8_t val_type;
		int32_t count;

		if (!readI08(val_type))
			return false;

		if (!readI32(count))
			return false;

		for (int32_t i = 0; i < count; i++)
		{
			if (!skip(val_type))
				return false;
		}

		break;
	}
	default://return false??
		break;
	}

	return true;
}

} // end namespace srpc

