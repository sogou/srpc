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

#ifndef __RPC_BASIC_H__
#define __RPC_BASIC_H__

#include <stdint.h>
#include <chrono>
#include <google/protobuf/message.h>
#include "rpc_buffer.h"
#include <vector>

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace srpc
{

static constexpr const char	   *SRPC_SCHEME				= "srpc";
static constexpr unsigned short	SRPC_DEFAULT_PORT		= 1412;

static constexpr const char	   *SRPC_SSL_SCHEME			= "srpcs";
static constexpr unsigned short	SRPC_SSL_DEFAULT_PORT	= 6462;

static constexpr size_t			RPC_BODY_SIZE_LIMIT		= 2LL * 1024 * 1024 * 1024;
static constexpr int			SRPC_MODULE_MAX			= 5;
static constexpr size_t			SRPC_SPANID_SIZE		= 8;
static constexpr size_t			SRPC_TRACEID_SIZE		= 16;

#ifndef htonll

#if __BYTE_ORDER == __LITTLE_ENDIAN

static inline uint64_t htonll(uint64_t x)
{
	return ((uint64_t)htonl(x & 0xFFFFFFFF) << 32) + htonl(x >> 32);
}

static inline uint64_t ntohll(uint64_t x)
{
	return ((uint64_t)ntohl(x & 0xFFFFFFFF) << 32) + ntohl(x >> 32);
}

#elif __BYTE_ORDER == __BIG_ENDIAN

static inline uint64_t htonll(uint64_t x)
{
	return x;
}

static inline uint64_t ntohll(uint64_t x)
{
	return x;
}

#else
# error "unknown byte order"
#endif

#endif

#define SRPC_JSON_OPTION_ADD_WHITESPACE		(1<<3)
#define SRPC_JSON_OPTION_ENUM_AS_INITS		(1<<4)
#define SRPC_JSON_OPTION_PRESERVE_NAMES		(1<<5)
#define SRPC_JSON_OPTION_PRINT_PRIMITIVE	(1<<6)

using ProtobufIDLMessage = google::protobuf::Message;
using RPCLogVector = std::vector<std::pair<std::string, std::string>>;

enum RPCDataType
{
	RPCDataUndefined	=	-1,
	RPCDataProtobuf		=	0,
	RPCDataThrift		=	1,
	RPCDataJson			=	2,
};

enum RPCStatusCode
{
	RPCStatusUndefined					=	0,
	RPCStatusOK							=	1,
	RPCStatusServiceNotFound			=	2,
	RPCStatusMethodNotFound				=	3,
	RPCStatusMetaError					=	4,

	RPCStatusReqCompressSizeInvalid		=	5,
	RPCStatusReqDecompressSizeInvalid	=	6,
	RPCStatusReqCompressNotSupported	=	7,
	RPCStatusReqDecompressNotSupported	=	8,
	RPCStatusReqCompressError			=	9,
	RPCStatusReqDecompressError			=	10,
	RPCStatusReqSerializeError			=	11,
	RPCStatusReqDeserializeError		=	12,
	RPCStatusRespCompressSizeInvalid	=	13,
	RPCStatusRespDecompressSizeInvalid	=	14,
	RPCStatusRespCompressNotSupported	=	15,
	RPCStatusRespDecompressNotSupported	=	16,
	RPCStatusRespCompressError			=	17,
	RPCStatusRespDecompressError		=	18,
	RPCStatusRespSerializeError			=	19,
	RPCStatusRespDeserializeError		=	20,
	RPCStatusIDLSerializeNotSupported	=	21,
	RPCStatusIDLDeserializeNotSupported	=	22,

	RPCStatusURIInvalid					=	30,
	RPCStatusUpstreamFailed				=	31,
	RPCStatusSystemError				=	100,
	RPCStatusSSLError					=	101,
	RPCStatusDNSError					=	102,
	RPCStatusProcessTerminated			=	103,
};

enum RPCCompressType
{
	RPCCompressNone		=	0,
	RPCCompressSnappy	=	1,
	RPCCompressGzip		=	2,
	RPCCompressZlib		=	3,
	RPCCompressLz4		=	4,
	RPCCompressMax		=	5,
};

enum RPCModuleType
{
	RPCModuleTypeEmpty		=	0,
	RPCModuleTypeTrace		=	1,
	RPCModuleTypeMetrics	=	2,
	RPCModuleTypeLog		=	3,
	RPCModuleTypeCustom		=	4,
};

class RPCCommon
{
public:
	static void log_format(std::string& key, std::string& value,
						   const RPCLogVector& fields);
	static bool addr_to_string(const struct sockaddr *addr, char *ip_str,
							   socklen_t len, unsigned short *port);
};

static inline long long GET_CURRENT_MS()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
};

static inline long long GET_CURRENT_MS_STEADY()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
}

static inline unsigned long long GET_CURRENT_NS()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

static inline void TRACE_ID_BIN_TO_HEX(const uint64_t trace_id[2],
									   char hex[SRPC_TRACEID_SIZE * 2 + 1])
{
	sprintf(hex, "%016llx%016llx", (unsigned long long)ntohll(trace_id[0]),
								   (unsigned long long)ntohll(trace_id[1]));
}

static inline void SPAN_ID_BIN_TO_HEX(const uint64_t span_id[1],
									  char hex[SRPC_SPANID_SIZE * 2 + 1])
{
	sprintf(hex, "%016llx", (unsigned long long)ntohll(span_id[0]));
}

static inline void TRACE_ID_HEX_TO_BIN(const char hex[SRPC_TRACEID_SIZE * 2 + 1],
									   uint64_t trace_id[2])
{
	sscanf(hex, "%016llx%016llx", (unsigned long long *)&trace_id[0],
								  (unsigned long long *)&trace_id[1]);
	trace_id[0] = ntohll(trace_id[0]);
	trace_id[1] = ntohll(trace_id[1]);
}

static inline void SPAN_ID_HEX_TO_BIN(const char hex[SRPC_SPANID_SIZE * 2 + 1],
									  uint64_t span_id[1])
{
	sscanf(hex, "%016llx", (unsigned long long *)&span_id[0]);
	span_id[0] = ntohll(span_id[0]);
}

} // end namespace srpc

#endif

