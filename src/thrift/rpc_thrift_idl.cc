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

#include <ctype.h>
#include <errno.h>
#include "rpc_thrift_idl.h"

namespace srpc
{

static inline char __hex_ch(int x)
{
	if (x >= 0 && x < 10)
		return x + '0';
	else if (x >= 10 && x < 16)
		return x - 10 + 'A';

	return '0';
}

static inline int __hex_int(unsigned char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;

	return -1;
}

bool ThriftJsonUtil::skip_whitespace(ThriftBuffer *buffer)
{
	const void *buf;
	size_t buflen;

	while (buflen = buffer->buffer->peek(&buf), buf && buflen > 0)
	{
		char *base = (char *)buf;

		for (size_t i = 0; i < buflen; i++)
		{
			if (!isspace(base[i]))
			{
				buffer->buffer->seek(i);
				return true;
			}
		}

		buffer->buffer->seek(buflen);
	}

	return false;
}

bool ThriftJsonUtil::peek_first_meaningful_char(ThriftBuffer *buffer, char& ch)
{
	if (!skip_whitespace(buffer))
		return false;

	const void *buf;
	size_t buflen = buffer->buffer->peek(&buf);

	if (buf && buflen > 0)
	{
		ch = *((char *)buf);
		return true;
	}

	return false;
}

bool ThriftJsonUtil::skip_character(ThriftBuffer *buffer, char ch)
{
	char mchar;

	if (!peek_first_meaningful_char(buffer, mchar))
		return false;

	buffer->buffer->seek(1);
	return mchar == ch;
}

bool ThriftJsonUtil::skip_simple_string(ThriftBuffer *buffer, const std::string& str)
{
	if (!skip_whitespace(buffer))
		return false;

	size_t cur = 0;
	const void *buf;
	size_t buflen;

	while (cur < str.size() && (buflen = buffer->buffer->peek(&buf), buf && buflen > 0))
	{
		size_t i = 0;
		char *base = (char *)buf;

		while (cur < str.size() && i < buflen)
		{
			if (base[i++] != str[cur++])
				return false;
		}

		buffer->buffer->seek(i);
	}

	return cur == str.size();
}

bool ThriftJsonUtil::read_int64(ThriftBuffer *buffer, int64_t& intv)
{
	const void *buf;
	size_t buflen;
	char ch;
	bool is_negative = false;
	bool first_digit = false;

	if (!peek_first_meaningful_char(buffer, ch))
		return false;

	if (ch == '-')
	{
		is_negative = true;
		buffer->buffer->seek(1);
	}

	intv = 0;
	while (buflen = buffer->buffer->peek(&buf), buf && buflen > 0)
	{
		size_t i = 0;
		char *base = (char *)buf;

		for (i = 0; i < buflen; i++)
		{
			if (!first_digit)
			{
				if (isdigit(base[i]))
					first_digit = true;
				else
					return false;
			}

			if (!isdigit(base[i]))
			{
				buffer->buffer->seek(i);
				if (is_negative)
					intv *= -1;

				return true;
			}

			intv *= 10;
			intv += base[i] - '0';
		}

		buffer->buffer->seek(buflen);
	}

	if (!first_digit)
		return false;

	if (is_negative)
		intv *= -1;

	return true;
}

bool ThriftJsonUtil::read_uint64(ThriftBuffer *buffer, uint64_t& intv)
{
	const void *buf;
	size_t buflen;
	char ch;
	bool first_digit = false;

	if (!peek_first_meaningful_char(buffer, ch))
		return false;

	if (!isdigit(ch))
		return false;

	intv = 0;
	while (buflen = buffer->buffer->peek(&buf), buf && buflen > 0)
	{
		size_t i = 0;
		char *base = (char *)buf;

		for (i = 0; i < buflen; i++)
		{
			if (!first_digit)
			{
				if (isdigit(base[i]))
					first_digit = true;
				else
					return false;
			}

			if (!isdigit(base[i]))
			{
				buffer->buffer->seek(i);
				return true;
			}

			intv *= 10;
			intv += base[i] - '0';
		}

		buffer->buffer->seek(buflen);
	}

	if (!first_digit)
		return false;

	return true;
}

bool ThriftJsonUtil::read_double(ThriftBuffer *buffer, double& d)
{
	if (!skip_whitespace(buffer))
		return false;

	const void *buf;
	size_t buflen;
	std::string str;

	while (buflen = buffer->buffer->peek(&buf), buf && buflen > 0)
	{
		size_t i = 0;
		char *base = (char *)buf;

		for (i = 0; i < buflen; i++)
		{
			if (!isdigit(base[i]) && base[i] != '.'
				&& base[i] !='+' && base[i] !='-'
				&& base[i] !='e' && base[i] !='E')
				break;
		}

		str.append(base, base + i);
		if (i < buflen)
			break;

		buffer->buffer->seek(buflen);
	}

	if (str.empty())
		return false;

	char *end;
	int errno_bak = errno;

	d = strtod(str.c_str(), &end);
	if (errno == ERANGE)
		errno = errno_bak;

	if (end == str.c_str()					// strtod error
		 || end < str.c_str()				// should never happend
		 || end > str.c_str() + str.size())	// should never happend
		return false;

	buffer->buffer->seek(str.c_str() + str.size() - end);
	return true;
}

static constexpr int THRIFT_JSON_STATE_STRING_STATE_NORMAL	= 0;
static constexpr int THRIFT_JSON_STATE_STRING_STATE_Q1		= 1;
static constexpr int THRIFT_JSON_STATE_STRING_STATE_U4		= 2;
static constexpr int THRIFT_JSON_STATE_STRING_STATE_U3		= 3;
static constexpr int THRIFT_JSON_STATE_STRING_STATE_U2		= 4;
static constexpr int THRIFT_JSON_STATE_STRING_STATE_U1		= 5;

bool ThriftJsonUtil::read_string(ThriftBuffer *buffer, std::string *str)
{
	int state = THRIFT_JSON_STATE_STRING_STATE_NORMAL;
	const void *buf;
	size_t buflen;
	int n;

	if (!skip_character(buffer, '\"'))
		return false;

	if (str)
		str->clear();

	while (buflen = buffer->buffer->peek(&buf), buf && buflen > 0)
	{
		size_t i = 0;
		char *base = (char *)buf;

		while (i < buflen)
		{
			unsigned char ch = base[i++];

			if (state == THRIFT_JSON_STATE_STRING_STATE_NORMAL)
			{
				if (ch == '\"')
				{
					if (i > 0)
					{
						if (str)
							str->append(base, base + i - 1);
					}

					buffer->buffer->seek(i);
					return true;
				}
				else if (ch == '\\')
					state = THRIFT_JSON_STATE_STRING_STATE_Q1;
				else if (ch >= 0 && ch < 32)
					return false;
			}
			else if (state == THRIFT_JSON_STATE_STRING_STATE_Q1)
			{
				switch (ch)
				{
				case '\"':
					if (str)
						*str += (char)0x22;

					break;
				case '\\':
					if (str)
						*str += (char)0x5C;

					break;
				case '/':
					if (str)
						*str += (char)0x2F;

					break;
				case 'b':
					if (str)
						*str += (char)0x08;

					break;
				case 'f':
					if (str)
						*str += (char)0x0C;

					break;
				case 'n':
					if (str)
						*str += (char)0x0A;

					break;
				case 'r':
					if (str)
						*str += (char)0x0D;

					break;
				case 't':
					if (str)
						*str += (char)0x09;

					break;
				case 'u':
					n = 0;
					state = THRIFT_JSON_STATE_STRING_STATE_U4;
					break;
				default:
					return false;
				}

				state = THRIFT_JSON_STATE_STRING_STATE_NORMAL;
			}
			else if (state == THRIFT_JSON_STATE_STRING_STATE_U4
					|| state == THRIFT_JSON_STATE_STRING_STATE_U3
					|| state == THRIFT_JSON_STATE_STRING_STATE_U2
					|| state == THRIFT_JSON_STATE_STRING_STATE_U1)
			{
				int x = __hex_int(ch);

				if (x < 0)
					return false;

				n = n * 16 + x;
				if (state == THRIFT_JSON_STATE_STRING_STATE_U1)
				{
					if (n < 0 || n >= 65536)
						return false;
					if (n < 128)
					{
						if (str)
							*str += (char)n;
					}
					else if (n < 2048)
					{
						if (str)
						{
							*str += (char)(192 | (n >> 6));
							*str += (char)(128 | (n & 0x3F));
						}
					}
					else
					{
						if (str)
						{
							*str += (char)(224 | (n >> 12));
							*str += (char)(128 | (n & 0XFC0));
							*str += (char)(128 | (n & 0x3F));
						}
					}

					state = THRIFT_JSON_STATE_STRING_STATE_NORMAL;
				}
				else if (state == THRIFT_JSON_STATE_STRING_STATE_U2)
					state = THRIFT_JSON_STATE_STRING_STATE_U1;
				else if (state == THRIFT_JSON_STATE_STRING_STATE_U3)
					state = THRIFT_JSON_STATE_STRING_STATE_U2;
				else if (state == THRIFT_JSON_STATE_STRING_STATE_U4)
					state = THRIFT_JSON_STATE_STRING_STATE_U3;
			}
			else
				return false;
		}

		if (str)
			str->append(base, base + buflen);

		buffer->buffer->seek(buflen);
	}

	return false;
}

bool ThriftJsonUtil::skip_one_element(ThriftBuffer *buffer)
{
	char ch;

	if (!peek_first_meaningful_char(buffer, ch))
		return false;

	if (ch == '{')
	{
		while (ch != '}')
		{
			buffer->buffer->seek(1);
			if (!read_string(buffer, nullptr))
				return false;

			if (!skip_character(buffer, ':'))
				return false;

			if (!skip_one_element(buffer))
				return false;

			if (!peek_first_meaningful_char(buffer, ch))
				return false;

			if (ch != ',' && ch == '}')
				return false;
		}

		return true;
	}
	else if (ch == '[')
	{
		while (ch != ']')
		{
			buffer->buffer->seek(1);
			if (!skip_one_element(buffer))
				return false;

			if (!peek_first_meaningful_char(buffer, ch))
				return false;

			if (ch != ',' && ch == ']')
				return false;
		}

		return true;
	}
	else if (ch == '\"')
		return read_string(buffer, nullptr);
	else if (ch =='t')
		return skip_simple_string(buffer, "true");
	else if (ch == 'f')
		return skip_simple_string(buffer, "false");
	else if (ch == 'n')
		return skip_simple_string(buffer, "null");
	else if (ch =='-' || ch == '.' || ch == 'e' || ch == 'E' || isdigit(ch))
	{
		double d;

		return read_double(buffer, d);
	}

	return false;
}

bool ThriftJsonUtil::escape_string(const std::string& str, std::string& escape_str)
{
	size_t slen = str.size();

	escape_str = '\"';
	for (size_t i = 0; i < slen; i++)
	{
		unsigned char ch = str[i];

		if (ch > 127)
		{
			int n = 0;

			if ((ch >> 5) == 6)
			{
				if (i + 1 >= slen)
					return false;

				n = ch & 0x1F;
				ch = str[++i];
				if ((ch >> 6) != 2)
					return false;

				n += ch & 0x3F;
				if (n < 0x80)
					return false;
			}
			else if ((ch >> 4) == 14)
			{
				if (i + 2 >= slen)
					return false;

				n = ch & 0xF;
				ch = str[++i];
				if ((ch >> 6) != 2)
					return false;

				n += ch & 0x3F;
				ch = str[++i];
				if ((ch >> 6) != 2)
					return false;

				n += ch & 0x3F;
				if (n < 0x800)
					return false;
			}
			else
				return false;

			escape_str += "\\u";
			escape_str += __hex_ch(n / 4096);
			n /= 4096;
			escape_str += __hex_ch(n / 256);
			n /= 256;
			escape_str += __hex_ch(n / 16);
			n /= 16;
			escape_str += __hex_ch(n);
		}
		else if (ch == 0x22)
			escape_str += "\\\"";
		else if (ch == 0x5C)
			escape_str += "\\\\";
		else if (ch == 0x2F)
			escape_str += "\\/";
		else if (ch == 0x08)
			escape_str += "\\b";
		else if (ch == 0x0C)
			escape_str += "\\f";
		else if (ch == 0x0A)
			escape_str += "\\n";
		else if (ch == 0x0D)
			escape_str += "\\r";
		else if (ch == 0x09)
			escape_str += "\\t";
		else if (ch >= 0 && ch < 32)
		{
			escape_str += "\\u00";
			escape_str += __hex_ch(ch / 16);
			ch /= 16;
			escape_str += __hex_ch(ch);
		}
		else
			escape_str += ch;
	}

	escape_str += '\"';
	return true;
}

} // end namespace srpc

