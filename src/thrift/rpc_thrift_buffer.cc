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

#include "rpc_thrift_buffer.h"

namespace sogou
{

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

} // end namespace sogou

