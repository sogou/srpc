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

#ifndef __RPC_THRIFT_ENUM_H__
#define __RPC_THRIFT_ENUM_H__

namespace srpc
{

static constexpr int THRIFT_STRUCT_FIELD_REQUIRED	= 0;
static constexpr int THRIFT_STRUCT_FIELD_OPTIONAL	= 1;
static constexpr int THRIFT_STRUCT_FIELD_DEFAULT	= 2;

enum ThriftMessageType
{
	TMT_CALL		= 1,
	TMT_REPLY		= 2,
	TMT_EXCEPTION	= 3,
	TMT_ONEWAY		= 4
};

enum ThriftDataType
{
	TDT_STOP		= 0,
	TDT_VOID		= 1,
	TDT_BOOL		= 2,
	TDT_BYTE		= 3,
	TDT_I08			= 3,
	TDT_I16			= 6,
	TDT_I32			= 8,
	TDT_U64			= 9,
	TDT_I64			= 10,
	TDT_DOUBLE		= 4,
	TDT_STRING		= 11,
	TDT_UTF7		= 11,
	TDT_STRUCT		= 12,
	TDT_MAP			= 13,
	TDT_SET			= 14,
	TDT_LIST		= 15,
	TDT_UTF8		= 16,
	TDT_UTF16		= 17
};

enum ThriftExceptionType
{
	TET_UNKNOWN					= 0,
	TET_UNKNOWN_METHOD			= 1,
	TET_INVALID_MESSAGE_TYPE	= 2,
	TET_WRONG_METHOD_NAME		= 3,
	TET_BAD_SEQUENCE_ID			= 4,
	TET_MISSING_RESULT			= 5,
	TET_INTERNAL_ERROR			= 6,
	TET_PROTOCOL_ERROR			= 7,
	TET_INVALID_TRANSFORM		= 8,
	TET_INVALID_PROTOCOL		= 9,
	TET_UNSUPPORTED_CLIENT_TYPE	= 10
};

} // end namespace srpc

#endif

