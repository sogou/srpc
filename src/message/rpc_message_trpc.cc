/*
  Copyright (c) 2021 Sogou, Inc.

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

#include <errno.h>
#include <vector>
#include <string>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <workflow/HttpUtil.h>
#include <workflow/StringUtil.h>
#include <workflow/json_parser.h>
#include "rpc_message_trpc.h"
#include "rpc_meta_trpc.pb.h"
#include "rpc_basic.h"
#include "rpc_compress.h"
#include "rpc_zero_copy_stream.h"
#include "rpc_module.h"

namespace srpc
{

namespace TRPCHttpHeaders
{
	const std::string CompressType		=	"Content-Encoding";
	const std::string DataType			=	"Content-Type";
	const std::string CallType			=	"trpc-call-type";
	const std::string RequestId			=	"trpc-request-id";
	const std::string Timeout			=	"trpc-timeout";
	const std::string Caller			=	"trpc-caller";
	const std::string Callee			=	"trpc-callee";
	const std::string Func				=	"trpc-func";
	const std::string Ret				=	"trpc-ret";
	const std::string FuncRet			=	"trpc-func-ret";
	const std::string ErrorMsg			=	"trpc-error-msg";
	const std::string MessageType		=	"trpc-message-type";
	const std::string TransInfo			=	"trpc-trans-info";
	const std::string SRPCStatus		=	"SRPC-Status";
	const std::string SRPCError			=	"SRPC-Error";
}

namespace TRPCHttpHeadersCode
{
	enum
	{
		Unknown = 0,
		CompressType,
		DataType,
		CallType,
		RequestId,
		Timeout,
		Caller,
		Callee,
		Func,
		Ret,
		FuncRet,
		ErrorMsg,
		MessageType,
		TransInfo,
		SRPCStatus,
		SRPCError
	};
}

struct CaseCmp
{
	bool operator()(const std::string& lhs, const std::string& rhs) const
	{
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

static int GetHttpHeadersCode(const std::string& header)
{
	static const std::map<std::string, int, CaseCmp> M =
	{
		{TRPCHttpHeaders::CompressType,	TRPCHttpHeadersCode::CompressType},
		{TRPCHttpHeaders::DataType,		TRPCHttpHeadersCode::DataType},
		{TRPCHttpHeaders::CallType,		TRPCHttpHeadersCode::CallType},
		{TRPCHttpHeaders::RequestId,	TRPCHttpHeadersCode::RequestId},
		{TRPCHttpHeaders::Timeout,		TRPCHttpHeadersCode::Timeout},
		{TRPCHttpHeaders::Caller,		TRPCHttpHeadersCode::Caller},
		{TRPCHttpHeaders::Callee,		TRPCHttpHeadersCode::Callee},
		{TRPCHttpHeaders::Func,			TRPCHttpHeadersCode::Func},
		{TRPCHttpHeaders::Ret,			TRPCHttpHeadersCode::Ret},
		{TRPCHttpHeaders::FuncRet,		TRPCHttpHeadersCode::FuncRet},
		{TRPCHttpHeaders::ErrorMsg,		TRPCHttpHeadersCode::ErrorMsg},
		{TRPCHttpHeaders::MessageType,	TRPCHttpHeadersCode::MessageType},
		{TRPCHttpHeaders::TransInfo,	TRPCHttpHeadersCode::TransInfo},
		{TRPCHttpHeaders::SRPCStatus,	TRPCHttpHeadersCode::SRPCStatus},
		{TRPCHttpHeaders::SRPCError,	TRPCHttpHeadersCode::SRPCError}
	};

	auto it = M.find(header);
	return it == M.end() ? TRPCHttpHeadersCode::Unknown : it->second;
}

static int GetHttpDataType(const std::string &type)
{
	static const std::unordered_map<std::string, int> M =
	{
		{"application/json",		RPCDataJson},
		{"application/x-protobuf",	RPCDataProtobuf},
		{"application/protobuf",	RPCDataProtobuf},
		{"application/pb",			RPCDataProtobuf},
		{"application/proto",		RPCDataProtobuf}
	};

	auto it = M.find(type);
	return it == M.end() ? RPCDataUndefined : it->second;
}

static std::string GetHttpDataTypeStr(int type)
{
	switch (type)
	{
	case RPCDataJson:
		return "application/json";
	case RPCDataProtobuf:
		return "application/proto";
	}

	return "";
}

static int GetHttpCompressType(const std::string &type)
{
	static const std::unordered_map<std::string, int> M =
	{
		{"identity",	RPCCompressNone},
		{"x-snappy",	RPCCompressSnappy},
		{"gzip",		RPCCompressGzip},
		{"deflate",		RPCCompressZlib},
		{"x-lz4",		RPCCompressLz4}
	};
	auto it = M.find(type);
	return it == M.end() ? RPCCompressNone : it->second;
}

static std::string GetHttpCompressTypeStr(int type)
{
	switch (type)
	{
		case RPCCompressNone:
			return "identity";
		case RPCCompressSnappy:
			return "x-snappy";
		case RPCCompressGzip:
			return "gzip";
		case RPCCompressZlib:
			return "deflate";
		case RPCCompressLz4:
			return "x-lz4";
	}

	return "";
}

static int Base64Encode(const std::string& in, std::string& out)
{
	size_t len = in.length();
	size_t base64_len = (len + 2) / 3 * 4;
	const unsigned char *f = (const unsigned char *)in.c_str();

	out.resize(base64_len + 1);
	EVP_EncodeBlock((unsigned char *)&out[0], f, len);
	out.pop_back();

	return 0;
}

static int Base64Decode(const char *in, size_t len, std::string& out)
{
	size_t origin_len = (len + 3) / 4 * 3;
	const unsigned char *f = (const unsigned char *)in;
	int ret;
	int zeros = 0;

	out.resize(origin_len);
	ret = EVP_DecodeBlock((unsigned char *)&out[0], f, len);

	if (ret < 0)
		return ret;

	while (len != 0 && in[--len] == '=')
		zeros++;

	if (zeros > 2)
		return -1;

	out.resize(ret - zeros);
	return 0;
}

static std::string JsonEscape(const std::string& in)
{
	std::string out;
	out.reserve(in.size());

	for (char c : in)
	{
		switch (c)
		{
		case '\"':
			out.append("\\\"");
			break;

		case '\\':
			out.append("\\\\");
			break;

		case '/':
			out.append("\\/");
			break;

		case '\b':
			out.append("\\b");
			break;

		case '\f':
			out.append("\\f");
			break;

		case '\n':
			out.append("\\n");
			break;

		case '\r':
			out.append("\\r");
			break;

		case '\t':
			out.append("\\t");
			break;

		default:
			out.push_back(c);
			break;
		}
	}

	return out;
}

using TransInfoMap = ::google::protobuf::Map<::std::string, ::std::string>;
static int DecodeTransInfo(const std::string& content, TransInfoMap& map)
{
	json_value_t *json;
	json_object_t *obj;
	const json_value_t *value;
	const char *name, *str;
	size_t len;
	std::string decoded;
	int errno_bak = errno;

	errno = EBADMSG;
	json = json_value_parse(content.c_str());
	if (json == nullptr)
		return -1;

	errno = errno_bak;

	if (json_value_type(json) != JSON_VALUE_OBJECT)
	{
		json_value_destroy(json);
		return -1;
	}

	obj = json_value_object(json);
	json_object_for_each(name, value, obj)
	{
		str = json_value_string(value);
		if (!name || !str)
			continue;

		len = strlen(str);
		if (Base64Decode(str, len, decoded) == 0)
			map[name].assign(std::move(decoded));
		else
			map[name].assign(str, len);
	}

	json_value_destroy(json);
	return 0;
}

static std::string EncodeTransInfo(const TransInfoMap& map)
{
	std::string s;
	std::string encoded;

	s.append("{");
	for (const auto& kv : map)
	{
		Base64Encode(kv.second, encoded);
		s.append("\"").append(JsonEscape(kv.first)).append("\":");
		s.append("\"").append(encoded).append("\",");
	}

	if (s.back() == ',')
		s.back() = '}';
	else
		s.push_back('}');

	return s;
}

static constexpr const char *kTypePrefix = "type.googleapis.com";

class ResolverInstance
{
public:
	static google::protobuf::util::TypeResolver *get_resolver()
	{
		static ResolverInstance kInstance;
		return kInstance.resolver_;
	}

private:
	ResolverInstance()
	{
		resolver_ = google::protobuf::util::NewTypeResolverForDescriptorPool(
			kTypePrefix, google::protobuf::DescriptorPool::generated_pool());
	}

	~ResolverInstance() { delete resolver_; }

	google::protobuf::util::TypeResolver *resolver_;
};

static inline std::string GetTypeUrl(const ProtobufIDLMessage *pb_msg)
{
	return std::string(kTypePrefix) + "/" + pb_msg->GetDescriptor()->full_name();
}

TRPCMessage::TRPCMessage()
{
	this->nreceived = 0;
	this->meta_buf = NULL;
	this->meta_len = 0;
	this->message_len = 0;
	memset(this->header, 0, TRPC_HEADER_SIZE);
	this->message = new RPCBuffer();
}

TRPCMessage::~TRPCMessage()
{
	delete this->message;
	delete []this->meta_buf;
	delete this->meta;
}

TRPCRequest::TRPCRequest()
{
	this->meta = new RequestProtocol();
}

TRPCResponse::TRPCResponse()
{
	this->meta = new ResponseProtocol();
}

int TRPCMessage::encode(struct iovec vectors[], int max, size_t size_limit)
{
	size_t sz = TRPC_HEADER_SIZE + this->meta_len + this->message_len;

	if (sz > 0x7FFFFFFF)
	{
		errno = EOVERFLOW;
		return -1;
	}

	int ret;
	char *p = this->header;

	*(uint16_t *)p = ntohs((uint16_t)TrpcMagic::TRPC_MAGIC_VALUE);
	p += 2;

	p += 1; // TrpcDataFrameType
	p += 1; // TrpcDataFrameState

	*(uint32_t *)(p) = htonl((uint32_t)sz);
	p += 4;

	*(uint16_t *)(p) = htons((uint16_t)this->meta_len);
	// 2: stream_id + 4 : reserved

	vectors[0].iov_base = this->header;
	vectors[0].iov_len = TRPC_HEADER_SIZE;

	vectors[1].iov_base = this->meta_buf;
	vectors[1].iov_len = this->meta_len;

	ret = this->message->encode(vectors + 2, max - 2);
	if (ret < 0)
		return ret;

	return 2 + ret;
}

int TRPCMessage::append(const void *buf, size_t *size, size_t size_limit)
{
	uint32_t *p;
	uint16_t *sp;
	size_t header_left, body_received, buf_len;
	if (this->nreceived < TRPC_HEADER_SIZE)
	{
		//receive header
		header_left = TRPC_HEADER_SIZE - this->nreceived;
		if (*size >= header_left)
		{
			//receive the whole header and ready to recieve body
			memcpy(this->header + this->nreceived, buf, header_left);
			this->nreceived += header_left;

			sp = (uint16_t *)this->header;
			uint16_t magic_value = ntohs(*sp);
			if (magic_value != TrpcMagic::TRPC_MAGIC_VALUE ||
				this->header[2] || this->header[3])
			{
				errno = EBADMSG;
				return -1;
			}

			p = (uint32_t *)this->header + 1;
			buf_len = ntohl(*p);
			sp = (uint16_t *)this->header + 4;
			this->meta_len = ntohs(*sp);

			this->message_len = buf_len - TRPC_HEADER_SIZE - this->meta_len;
			buf_len -= TRPC_HEADER_SIZE;

			if (buf_len >= size_limit)
			{
				errno = EMSGSIZE;
				return -1;
			}
			else if (buf_len > 0)
			{
				if (*size - header_left > buf_len)
					*size = header_left + buf_len;

				this->meta_buf = new char[this->meta_len];

				if (*size - header_left <= this->meta_len)
				{
					memcpy(this->meta_buf, (const char *)buf + header_left,
						   *size - header_left);
				} else {
					memcpy(this->meta_buf, (const char *)buf + header_left,
						   this->meta_len);

					this->message->append((const char *)buf + header_left + this->meta_len,
										  *size - header_left - this->meta_len,
										  BUFFER_MODE_COPY);
				}

				this->nreceived += *size - header_left;
				if (this->nreceived == TRPC_HEADER_SIZE + buf_len)
					return 1;
				else
					return 0;
			}
			else if (*size == header_left)
			{
				return 1; // means body_size == 0 and finish recieved header
			}
			else
			{
				// means buf_len < 0
				errno = EBADMSG;
				return -1;
			}
		}
		else
		{
			// only receive header
			memcpy(this->header + this->nreceived, buf, *size);
			this->nreceived += *size;
			return 0;
		}
	}
	else
	{
		// have already received the header and now is for body only
		body_received = this->nreceived - TRPC_HEADER_SIZE;
		buf_len = this->meta_len + this->message_len;
		if (body_received + *size > buf_len)
			*size = buf_len - body_received;

		if (body_received + *size <= this->meta_len)
		{
			memcpy(this->meta_buf + body_received, buf, *size);
		} else if (body_received < this->meta_len) {
			memcpy(this->meta_buf + body_received, buf,
				   this->meta_len - body_received);

			if (body_received + *size > this->meta_len)// useless. always true
				this->message->append((const char *)buf + this->meta_len - body_received,
									  *size - this->meta_len + body_received,
									  BUFFER_MODE_COPY);
		} else {
			this->message->append((const char *)buf, *size, BUFFER_MODE_COPY);
		}

		this->nreceived += *size;
		return this->nreceived == TRPC_HEADER_SIZE + buf_len;
	}
}

bool TRPCRequest::deserialize_meta()
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	if (!meta->ParseFromArray(this->meta_buf, (int)this->meta_len))
		return false;

	if (meta->version() != TrpcProtoVersion::TRPC_PROTO_V1 ||
		meta->call_type() != TrpcCallType::TRPC_UNARY_CALL ||
		meta->content_type() != TrpcContentEncodeType::TRPC_PROTO_ENCODE)
	{
		// this->trpc_error = ST_ERR_UNSUPPORTED_PROTO_TYPE;
		return false;
	}

//	this->timeout = meta->timeout();
	return true;
}

bool TRPCResponse::deserialize_meta()
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	if (!meta->ParseFromArray(this->meta_buf, (int)this->meta_len))
		return false;

	this->srpc_status_code = this->status_code_trpc_srpc(meta->ret());
	if (!meta->error_msg().empty())
		this->srpc_error_msg = meta->error_msg().data();

	return true;
}

bool TRPCRequest::serialize_meta()
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);
	meta->set_content_type(TrpcContentEncodeType::TRPC_PROTO_ENCODE);
	meta->set_version(TrpcProtoVersion::TRPC_PROTO_V1);
	meta->set_call_type(TrpcCallType::TRPC_UNARY_CALL);

	this->meta_len = meta->ByteSizeLong();
	this->meta_buf = new char[this->meta_len];
	return this->meta->SerializeToArray(this->meta_buf, (int)this->meta_len);
}

bool TRPCResponse::serialize_meta()
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);
	meta->set_version(TrpcProtoVersion::TRPC_PROTO_V1);
	meta->set_call_type(TrpcCallType::TRPC_UNARY_CALL);

	meta->set_ret(this->status_code_srpc_trpc(this->srpc_status_code));
//	meta->set_error_msg(this->error_msg_srpc_trpc(this->srpc_status_code));
	meta->set_error_msg(this->srpc_error_msg);

	this->meta_len = meta->ByteSizeLong();
	this->meta_buf = new char[this->meta_len];
	return meta->SerializeToArray(this->meta_buf, (int)this->meta_len);
}

inline int TRPCMessage::compress_type_trpc_srpc(int trpc_content_encoding) const
{
	switch (trpc_content_encoding)
	{
		case TrpcCompressType::TRPC_DEFAULT_COMPRESS :
			return RPCCompressNone;
		case TrpcCompressType::TRPC_GZIP_COMPRESS :
			return RPCCompressGzip;
		case TrpcCompressType::TRPC_SNAPPY_COMPRESS :
			return RPCCompressSnappy;
		default :
			return -1;
	}
}

inline int TRPCMessage::compress_type_srpc_trpc(int srpc_compress_type) const
{
	switch (srpc_compress_type)
	{
		case RPCCompressNone :
			return TrpcCompressType::TRPC_DEFAULT_COMPRESS;
		case RPCCompressGzip :
			return TrpcCompressType::TRPC_GZIP_COMPRESS;
		case RPCCompressSnappy :
			return TrpcCompressType::TRPC_SNAPPY_COMPRESS;
		case RPCCompressZlib :
			return TrpcCompressType::TRPC_ZLIB_COMPRESS;
		case RPCCompressLz4 :
			return TrpcCompressType::TRPC_LZ4_COMPRESS;
		default :
			return -1;
	}
}

inline int TRPCMessage::data_type_trpc_srpc(int trpc_content_type) const
{
	switch (trpc_content_type)
	{
		case TrpcContentEncodeType::TRPC_PROTO_ENCODE :
			return RPCDataProtobuf;
		case TrpcContentEncodeType::TRPC_JSON_ENCODE :
			return RPCDataJson;
		default :
			return -1;
	}
}

inline int TRPCMessage::data_type_srpc_trpc(int srpc_data_type) const
{
	switch (srpc_data_type)
	{
		case RPCDataProtobuf :
			return TrpcContentEncodeType::TRPC_PROTO_ENCODE;
		case RPCDataJson :
			return TrpcContentEncodeType::TRPC_JSON_ENCODE;
		default :
			return -1;
	}
}

inline int TRPCMessage::status_code_srpc_trpc(int srpc_status_code) const
{
	switch (srpc_status_code)
	{
	case RPCStatusOK:
		return TrpcRetCode::TRPC_INVOKE_SUCCESS;
	case RPCStatusServiceNotFound:
		return TrpcRetCode::TRPC_SERVER_NOSERVICE_ERR;
	case RPCStatusMethodNotFound:
		return TrpcRetCode::TRPC_SERVER_NOFUNC_ERR;
	case RPCStatusURIInvalid:
	case RPCStatusMetaError:
	case RPCStatusReqCompressSizeInvalid:
	case RPCStatusReqCompressNotSupported:
	case RPCStatusReqCompressError:
	case RPCStatusReqSerializeError:
		return TrpcRetCode::TRPC_CLIENT_ENCODE_ERR;
	case RPCStatusReqDecompressSizeInvalid:
	case RPCStatusReqDecompressNotSupported:
	case RPCStatusReqDecompressError:
	case RPCStatusReqDeserializeError:
		return TrpcRetCode::TRPC_SERVER_DECODE_ERR;
	case RPCStatusRespCompressSizeInvalid:
	case RPCStatusRespCompressNotSupported:
	case RPCStatusRespCompressError:
	case RPCStatusRespSerializeError:
		return TrpcRetCode::TRPC_SERVER_ENCODE_ERR;
	case RPCStatusRespDecompressSizeInvalid:
	case RPCStatusRespDecompressNotSupported:
	case RPCStatusRespDecompressError:
	case RPCStatusRespDeserializeError:
		return TrpcRetCode::TRPC_CLIENT_DECODE_ERR;
	case RPCStatusUpstreamFailed:
	case RPCStatusDNSError:
		return TrpcRetCode::TRPC_CLIENT_ROUTER_ERR;
	case RPCStatusSystemError:
		return TrpcRetCode::TRPC_SERVER_SYSTEM_ERR;
//		return TrpcRetCode::TRPC_CLINET_NETWORK_ERR;
	default:
		return TrpcRetCode::TRPC_INVOKE_UNKNOWN_ERR;
	}
}

inline int TRPCMessage::status_code_trpc_srpc(int trpc_ret_code) const
{
	switch (trpc_ret_code)
	{
	case TrpcRetCode::TRPC_INVOKE_SUCCESS:
		return RPCStatusOK;
	case TrpcRetCode::TRPC_SERVER_NOSERVICE_ERR:
		return RPCStatusServiceNotFound;
	case TrpcRetCode::TRPC_SERVER_NOFUNC_ERR:
		return RPCStatusMethodNotFound;
	case TrpcRetCode::TRPC_CLIENT_ENCODE_ERR:
		return RPCStatusReqSerializeError;
	case TrpcRetCode::TRPC_SERVER_DECODE_ERR:
		return RPCStatusReqDeserializeError;
	case TrpcRetCode::TRPC_SERVER_ENCODE_ERR:
		return RPCStatusRespSerializeError;
	case TrpcRetCode::TRPC_CLIENT_DECODE_ERR:
		return RPCStatusRespDeserializeError;
	case TrpcRetCode::TRPC_CLIENT_ROUTER_ERR:
		return RPCStatusUpstreamFailed;
//		return RPCStatusDNSError;
	default:
		return RPCStatusSystemError;
	}
}

const char *TRPCMessage::error_msg_srpc_trpc(int srpc_status_code) const
{
	return "";//TODO
}

int TRPCRequest::get_compress_type() const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	return this->compress_type_trpc_srpc(meta->content_encoding());
}

void TRPCRequest::set_compress_type(int type)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	meta->set_content_encoding(this->compress_type_srpc_trpc(type));
}

int TRPCRequest::get_data_type() const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	return this->data_type_trpc_srpc(meta->content_type());
}

void TRPCRequest::set_data_type(int type)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	meta->set_content_type(this->data_type_srpc_trpc(type));
}

void TRPCRequest::set_request_id(int32_t req_id)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	meta->set_request_id(req_id);
}

int32_t TRPCRequest::get_request_id() const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	return meta->request_id();
}

const std::string& TRPCRequest::get_service_name() const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	return meta->callee();
}

const std::string& TRPCRequest::get_method_name() const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	return meta->func();
}

void TRPCRequest::set_service_name(const std::string& service_name)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	meta->set_callee(service_name);
}

void TRPCRequest::set_method_name(const std::string& method_name)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	meta->set_func(method_name);
}

void TRPCRequest::set_callee_timeout(int timeout)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	meta->set_timeout(timeout);
}

void TRPCRequest::set_caller_name(const std::string& caller_name)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	meta->set_caller(caller_name);
}

const std::string& TRPCRequest::get_caller_name() const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	return meta->caller();
}

int TRPCResponse::get_compress_type() const
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	return this->compress_type_trpc_srpc(meta->content_encoding());
}

int TRPCResponse::get_data_type() const
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	return this->data_type_trpc_srpc(meta->content_type());
}

void TRPCResponse::set_compress_type(int type)
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	meta->set_content_encoding(this->compress_type_srpc_trpc(type));
}

void TRPCResponse::set_data_type(int type)
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	meta->set_content_type(this->data_type_srpc_trpc(type));
}

void TRPCResponse::set_request_id(int32_t req_id)
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	meta->set_request_id(req_id);
}

int32_t TRPCResponse::get_request_id() const
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	return meta->request_id();
}

int TRPCResponse::get_status_code() const
{
	return this->srpc_status_code;
}

void TRPCResponse::set_status_code(int code)
{
	this->srpc_status_code = code;
	if (code != RPCStatusOK)
		this->srpc_error_msg = this->get_errmsg();
}

int TRPCResponse::get_error() const
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	return meta->ret();//TODO
}

void TRPCResponse::set_error(int error)
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	meta->set_ret(error);
}

const char *TRPCResponse::get_errmsg() const
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	return meta->error_msg().c_str();
}

int TRPCMessage::serialize(const ProtobufIDLMessage *pb_msg)
{
	if (!pb_msg) //TODO: make sure trpc is OK to send a NULL user pb_msg
		return RPCStatusOK;

	using namespace google::protobuf;
	ResponseProtocol *meta = dynamic_cast<ResponseProtocol *>(this->meta);
	bool is_resp = (meta != NULL);

	int data_type = this->get_data_type();
	RPCOutputStream output_stream(this->message, pb_msg->ByteSizeLong());
	int ret;

	if (data_type == RPCDataProtobuf)
		ret = pb_msg->SerializeToZeroCopyStream(&output_stream) ? 0 : -1;
	else if (data_type == RPCDataJson)
	{
		std::string binary_input = pb_msg->SerializeAsString();
		io::ArrayInputStream input_stream(binary_input.data(),
										  (int)binary_input.size());
		const auto *pool = pb_msg->GetDescriptor()->file()->pool();
		auto *resolver = (pool == DescriptorPool::generated_pool() ?
					ResolverInstance::get_resolver() :
					util::NewTypeResolverForDescriptorPool(kTypePrefix, pool));

		util::JsonOptions options;
		options.add_whitespace = this->get_json_add_whitespace();
		options.always_print_enums_as_ints = this->get_json_enums_as_ints();
		options.preserve_proto_field_names = this->get_json_preserve_names();
		options.always_print_primitive_fields = this->get_json_print_primitive();

		ret = BinaryToJsonStream(resolver, GetTypeUrl(pb_msg), &input_stream,
								 &output_stream, options).ok() ? 0 : -1;
		if (pool != DescriptorPool::generated_pool())
			delete resolver;
	}
	else
		ret = -1;

	this->message_len = this->message->size();

	if (ret < 0)
		return is_resp ? RPCStatusRespSerializeError :
						 RPCStatusReqSerializeError;

	return RPCStatusOK;
}

int TRPCMessage::deserialize(ProtobufIDLMessage *pb_msg)
{
	using namespace google::protobuf;
	ResponseProtocol *meta = dynamic_cast<ResponseProtocol *>(this->meta);
	bool is_resp = (meta != NULL);
	int data_type = this->get_data_type();
	int ret;

	RPCInputStream input_stream(this->message);

	if (data_type == RPCDataProtobuf)
		ret = pb_msg->ParseFromZeroCopyStream(&input_stream) ? 0 : -1;
	else if (data_type == RPCDataJson)
	{
		std::string binary_output;
		io::StringOutputStream output_stream(&binary_output);
		const auto *pool = pb_msg->GetDescriptor()->file()->pool();
		auto *resolver = (pool == DescriptorPool::generated_pool() ?
					ResolverInstance::get_resolver() :
					util::NewTypeResolverForDescriptorPool(kTypePrefix, pool));

		if (JsonToBinaryStream(resolver, GetTypeUrl(pb_msg),
							   &input_stream, &output_stream).ok())
		{
			ret = pb_msg->ParseFromString(binary_output) ? 0 : -1;
		}
		else
			ret = -1;

		if (pool != DescriptorPool::generated_pool())
			delete resolver;
	}
	else
		ret = -1;

	if (ret < 0)
		return is_resp ? RPCStatusRespDeserializeError :
						 RPCStatusReqDeserializeError;

	return RPCStatusOK;
}

int TRPCMessage::compress()
{
	ResponseProtocol *meta = dynamic_cast<ResponseProtocol *>(this->meta);
	bool is_resp = (meta != NULL);
	int type = this->get_compress_type();
	size_t buflen = this->message_len;
	int status_code = RPCStatusOK;

	if (buflen == 0)
		return status_code;

	if (type == RPCCompressNone)
		return status_code;

	static RPCCompressor *compressor = RPCCompressor::get_instance();
	int ret = compressor->lease_compressed_size(type, buflen);

	if (ret == -2)
		return is_resp ? RPCStatusReqCompressNotSupported :
						 RPCStatusRespCompressNotSupported;
	else if (ret <= 0)
		return is_resp ? RPCStatusRespCompressSizeInvalid :
						 RPCStatusReqCompressSizeInvalid;

	//buflen = ret;
	RPCBuffer *dst_buf = new RPCBuffer();
	ret = compressor->serialize_to_compressed(this->message, dst_buf, type);

	if (ret == -2)
	{
		status_code = is_resp ? RPCStatusRespCompressNotSupported :
								RPCStatusReqCompressNotSupported;
	}
	else if (ret == -1)
	{
		status_code = is_resp ? RPCStatusRespCompressError :
								RPCStatusReqCompressError;
	}
	else if (ret <= 0)
	{
		status_code = is_resp ? RPCStatusRespCompressSizeInvalid :
								RPCStatusReqCompressSizeInvalid;
	}
	else
		buflen = ret;

	if (status_code == RPCStatusOK)
	{
		delete this->message;
		this->message = dst_buf;
		this->message_len = buflen;
	}
	else
		delete dst_buf;

	return status_code;
}

int TRPCMessage::decompress()
{
	ResponseProtocol *meta = dynamic_cast<ResponseProtocol *>(this->meta);
	bool is_resp = (meta != NULL);
	int type = this->get_compress_type();
	int status_code = RPCStatusOK;

	if (this->message_len == 0 || type == RPCCompressNone)
		return status_code;

	RPCBuffer *dst_buf = new RPCBuffer();
	static RPCCompressor *compressor = RPCCompressor::get_instance();
	int ret = compressor->parse_from_compressed(this->message, dst_buf, type);

	if (ret == -2)
	{
		status_code = is_resp ? RPCStatusRespDecompressNotSupported :
								RPCStatusReqDecompressNotSupported;
	}
	else if (ret == -1)
	{
		status_code = is_resp ? RPCStatusRespDecompressError :
								RPCStatusReqDecompressError;
	}
	else if (ret <= 0)
	{
		status_code = is_resp ? RPCStatusRespDecompressSizeInvalid :
								RPCStatusReqDecompressSizeInvalid;
	}

	if (status_code == RPCStatusOK)
	{
		delete this->message;
		this->message = dst_buf;
		this->message_len = ret;
	}
	else
		delete dst_buf;

	return status_code;
}

static std::string set_trace_parent(std::string& trace, std::string& span,
									const RPCModuleData& data)
{
	std::string str = "00-"; // set version

	char trace_id_buf[SRPC_TRACEID_SIZE * 2 + 1];
	TRACE_ID_BIN_TO_HEX((uint64_t *)trace.data(), trace_id_buf);
	str.append(trace_id_buf);
	str.append("-");

	char span_id_buf[SRPC_SPANID_SIZE * 2 + 1];
	SPAN_ID_BIN_TO_HEX((uint64_t *)span.data(), span_id_buf);
	str.append(span_id_buf);
	str.append("-");

	str.append("01"); // set traceflag : sampled

	return str;
}

bool TRPCRequest::set_meta_module_data(const RPCModuleData& data)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);
	std::string trace;
	std::string span;
	int flag = 0;

	for (auto & pair : data)
	{
		if (pair.first.compare(SRPC_TRACE_ID) == 0)
		{
			trace = pair.second;
			flag |= 1;
		}
		else if (pair.first.compare(SRPC_SPAN_ID) == 0)
		{
			span = pair.second;
			flag |= (1 << 1);
		}
		else
			meta->mutable_trans_info()->insert({pair.first, pair.second});
	}

	if (flag == 3)
		meta->mutable_trans_info()->insert({OTLP_TRACE_PARENT,
											set_trace_parent(trace, span, data)});
	return true;
}

static bool get_trace_parent(const std::string& str, RPCModuleData& data)
{
	// only support version = 00 : "00-" trace-id "-" parent-id "-" trace-flags
	size_t begin = OTLP_TRACE_VERSION_SIZE;

	if (str.length() < 55 || str.substr(0, begin).compare("00") != 0)
		return false;

//	data[OTLP_TRACE_VERSION] = "00";

	uint64_t trace[2];
	begin += 1;
	TRACE_ID_HEX_TO_BIN(str.substr(begin, SRPC_TRACEID_SIZE * 2).data(), trace);
	data[SRPC_TRACE_ID] = std::string((char *)trace, SRPC_TRACEID_SIZE);

	uint64_t span[1];
	begin += SRPC_TRACEID_SIZE * 2 + 1;
	SPAN_ID_HEX_TO_BIN(str.substr(begin, SRPC_SPANID_SIZE * 2).data(), span);
	data[SRPC_SPAN_ID] = std::string((char *)span, SRPC_SPANID_SIZE);

//	begin += SRPC_SPANID_SIZE + 1;
//	data[OTLP_TRACE_FLAG] = str.substr(begin);

	return true;
}

bool TRPCRequest::get_meta_module_data(RPCModuleData& data) const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	for (auto & pair : meta->trans_info())
	{
		if (pair.first.compare(OTLP_TRACE_PARENT) == 0)
			get_trace_parent(pair.second, data);
		else if (pair.first.compare(OTLP_TRACE_STATE) == 0)
			;// TODO: support tracestate
		else
			data.insert(pair);
	}

	return true;
}

bool TRPCRequest::trim_method_prefix()
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);
	std::string *method = meta->mutable_func();

	auto pos = method->find_last_of('/');
	if (pos == std::string::npos)
		return false;

	meta->set_func(method->substr(pos + 1, method->length()));
	return true;
}

bool TRPCResponse::set_meta_module_data(const RPCModuleData& data)
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	for (auto & pair : data)
		meta->mutable_trans_info()->insert({pair.first, pair.second});

	return true;
}

bool TRPCResponse::get_meta_module_data(RPCModuleData& data) const
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);

	for (auto & pair : meta->trans_info())
		data.insert(pair);

	return true;
}

bool TRPCHttpRequest::serialize_meta()
{
	if (this->message->size() > 0x7FFFFFFF)
		return false;

	int data_type = this->get_data_type();
	int compress_type = this->get_compress_type();

	std::string uri("/");
	uri += this->get_service_name();
	uri += "/";
	uri += this->get_method_name();

	set_http_version("HTTP/1.1");
	set_method("POST");
	set_request_uri(uri);

	//set header
	set_header_pair(TRPCHttpHeaders::DataType,
					GetHttpDataTypeStr(data_type));

	set_header_pair(TRPCHttpHeaders::CompressType,
					GetHttpCompressTypeStr(compress_type));

	set_header_pair("Connection", "Keep-Alive");
	set_header_pair("Content-Length", std::to_string(this->message_len));

	set_header_pair(TRPCHttpHeaders::CallType, "0");
	set_header_pair(TRPCHttpHeaders::RequestId,
					std::to_string(this->get_request_id()));
	set_header_pair(TRPCHttpHeaders::Callee, this->get_method_name());
	set_header_pair(TRPCHttpHeaders::Func, this->get_service_name());
	set_header_pair(TRPCHttpHeaders::Caller, this->get_caller_name());

	auto *req_meta = (RequestProtocol *)this->meta;
	set_header_pair(TRPCHttpHeaders::TransInfo,
					EncodeTransInfo(req_meta->trans_info()));

	const void *buffer;
	size_t buflen;

	while (buflen = this->message->fetch(&buffer), buffer && buflen > 0)
		this->append_output_body_nocopy(buffer, buflen);

	return true;
}

bool TRPCHttpRequest::deserialize_meta()
{
	const char *request_uri = this->get_request_uri();
	protocol::HttpHeaderCursor header_cursor(this);
	auto *meta = (RequestProtocol *)this->meta;
	std::string key, value;

	this->set_data_type(RPCDataJson);
	this->set_compress_type(RPCCompressNone);

	while (header_cursor.next(key, value))
	{
		switch (GetHttpHeadersCode(key))
		{
		case TRPCHttpHeadersCode::DataType:
			this->set_data_type(GetHttpDataType(value));
			break;
		case TRPCHttpHeadersCode::CompressType:
			this->set_compress_type(GetHttpCompressType(value));
			break;
		case TRPCHttpHeadersCode::CallType:
			meta->set_call_type(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::RequestId:
			meta->set_request_id(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::Timeout:
			meta->set_timeout(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::Caller:
			meta->set_caller(value);
			break;
		case TRPCHttpHeadersCode::Callee:
			meta->set_callee(value);
			break;
		case TRPCHttpHeadersCode::MessageType:
			meta->set_message_type(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::TransInfo:
			DecodeTransInfo(value, *meta->mutable_trans_info());
			break;
		default:
			break;
		}
	}

	if (request_uri && request_uri[0] == '/')
	{
		std::string str = request_uri + 1;
		auto pos = str.find_first_of("?#");

		if (pos != std::string::npos)
			str.erase(pos);

		if (!str.empty() && str.back() == '/')
			str.pop_back();

		pos = str.find_last_of('/');
		if (pos != std::string::npos)
		{
			this->set_service_name(str.substr(0, pos));
			this->set_method_name(str.substr(pos + 1));
		}
	}

	const void *ptr;
	size_t len;

	this->get_parsed_body(&ptr, &len);
	if (len > 0x7FFFFFFF)
		return false;

	protocol::HttpChunkCursor chunk_cursor(this);
	RPCBuffer *buf = this->get_buffer();
	size_t msg_len = 0;

	while (chunk_cursor.next(&ptr, &len))
	{
		msg_len += len;
		buf->append((const char *)ptr, len, BUFFER_MODE_NOCOPY);
	}

	if (this->get_compress_type() == RPCCompressNone &&
		msg_len == 0 && this->get_data_type() == RPCDataJson)
	{
		buf->append("{}", 2, BUFFER_MODE_NOCOPY);
		msg_len = 2;
	}

	this->message_len = msg_len;

	return true;
}

bool TRPCHttpResponse::serialize_meta()
{
	if (this->message->size() > 0x7FFFFFFF)
		return false;

	auto *meta = (ResponseProtocol *)this->meta;
	int data_type = this->get_data_type();
	int compress_type = this->get_compress_type();
	int rpc_status_code = this->get_status_code();
	int rpc_error = this->get_error();
	const char *http_status_code = this->protocol::HttpResponse::get_status_code();

	set_http_version("HTTP/1.1");
	if (rpc_status_code == RPCStatusOK)
	{
		if (http_status_code)
			protocol::HttpUtil::set_response_status(this, atoi(http_status_code));
		else
			protocol::HttpUtil::set_response_status(this, HttpStatusOK);
	}
	else if (rpc_status_code == RPCStatusServiceNotFound
			|| rpc_status_code == RPCStatusMethodNotFound
			|| rpc_status_code == RPCStatusMetaError
			|| rpc_status_code == RPCStatusURIInvalid)
	{
		protocol::HttpUtil::set_response_status(this, HttpStatusBadRequest);
	}
	else if (rpc_status_code == RPCStatusRespCompressNotSupported
			|| rpc_status_code == RPCStatusRespDecompressNotSupported
			|| rpc_status_code == RPCStatusIDLSerializeNotSupported
			|| rpc_status_code == RPCStatusIDLDeserializeNotSupported)
	{
		protocol::HttpUtil::set_response_status(this, HttpStatusNotImplemented);
	}
	else if (rpc_status_code == RPCStatusUpstreamFailed)
	{
		protocol::HttpUtil::set_response_status(this,
												HttpStatusServiceUnavailable);
	}
	else
	{
		protocol::HttpUtil::set_response_status(this,
												HttpStatusInternalServerError);
	}

	//set header
	set_header_pair(TRPCHttpHeaders::SRPCStatus,
					std::to_string(rpc_status_code));

	set_header_pair(TRPCHttpHeaders::SRPCError,
					std::to_string(rpc_error));

	set_header_pair(TRPCHttpHeaders::DataType,
					GetHttpDataTypeStr(data_type));

	set_header_pair(TRPCHttpHeaders::CompressType,
					GetHttpCompressTypeStr(compress_type));

	set_header_pair(TRPCHttpHeaders::CallType,
					std::to_string(meta->call_type()));

	set_header_pair(TRPCHttpHeaders::RequestId,
					std::to_string(meta->request_id()));

	set_header_pair(TRPCHttpHeaders::Ret,
					std::to_string(meta->ret()));

	set_header_pair(TRPCHttpHeaders::FuncRet,
					std::to_string(meta->func_ret()));

	set_header_pair(TRPCHttpHeaders::ErrorMsg,
					meta->error_msg());

	set_header_pair(TRPCHttpHeaders::MessageType,
					std::to_string(meta->message_type()));

	set_header_pair("Content-Length", std::to_string(this->message_len));

	set_header_pair("Connection", "Keep-Alive");

	set_header_pair(TRPCHttpHeaders::TransInfo,
					EncodeTransInfo(meta->trans_info()));

	const void *buffer;
	size_t buflen;

	while (buflen = this->message->fetch(&buffer), buffer && buflen > 0)
		this->append_output_body_nocopy(buffer, buflen);

	return true;
}

bool TRPCHttpResponse::deserialize_meta()
{
	protocol::HttpHeaderCursor header_cursor(this);
	auto *meta = (ResponseProtocol *)this->meta;
	std::string key, value;

	this->set_data_type(RPCDataJson);
	this->set_compress_type(RPCCompressNone);

	while (header_cursor.next(key, value))
	{
		switch (GetHttpHeadersCode(key))
		{
		case TRPCHttpHeadersCode::DataType:
			this->set_data_type(GetHttpDataType(value));
			break;
		case TRPCHttpHeadersCode::CompressType:
			this->set_compress_type(GetHttpCompressType(value));
			break;
		case TRPCHttpHeadersCode::CallType:
			meta->set_call_type(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::RequestId:
			meta->set_request_id(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::Ret:
			meta->set_ret(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::FuncRet:
			meta->set_func_ret(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::ErrorMsg:
			meta->set_error_msg(value);
			break;
		case TRPCHttpHeadersCode::MessageType:
			meta->set_message_type(std::atoi(value.c_str()));
			break;
		case TRPCHttpHeadersCode::TransInfo:
			DecodeTransInfo(value, *meta->mutable_trans_info());
			break;
		default:
			break;
		}
	}

	const void *ptr;
	size_t len;

	this->get_parsed_body(&ptr, &len);
	if (len > 0x7FFFFFFF)
		return false;

	protocol::HttpChunkCursor chunk_cursor(this);
	RPCBuffer *buf = this->get_buffer();
	size_t msg_len = 0;

	while (chunk_cursor.next(&ptr, &len))
	{
		buf->append((const char *)ptr, len, BUFFER_MODE_NOCOPY);
		msg_len += len;
	}

	this->message_len = msg_len;

	return true;
}

bool TRPCHttpRequest::set_meta_module_data(const RPCModuleData& data)
{
	return this->TRPCRequest::set_meta_module_data(data);
}

bool TRPCHttpRequest::get_meta_module_data(RPCModuleData& data) const
{
	return this->TRPCRequest::get_meta_module_data(data);
}

bool TRPCHttpResponse::set_meta_module_data(const RPCModuleData& data)
{
	return this->TRPCResponse::set_meta_module_data(data);
}

bool TRPCHttpResponse::get_meta_module_data(RPCModuleData& data) const
{
	return this->TRPCResponse::get_meta_module_data(data);
}

bool TRPCHttpRequest::set_http_header(const std::string& name,
									  const std::string& value)
{
	return this->protocol::HttpMessage::set_header_pair(name, value);
}

bool TRPCHttpRequest::add_http_header(const std::string& name,
									  const std::string& value)
{
	return this->protocol::HttpMessage::add_header_pair(name, value);
}

bool TRPCHttpRequest::get_http_header(const std::string& name,
									  std::string& value) const
{
	protocol::HttpHeaderCursor cursor(this);
	return cursor.find(name, value);
}

bool TRPCHttpResponse::set_http_header(const std::string& name,
									   const std::string& value)
{
	return this->protocol::HttpMessage::set_header_pair(name, value);
}

bool TRPCHttpResponse::add_http_header(const std::string& name,
									   const std::string& value)
{
	return this->protocol::HttpMessage::add_header_pair(name, value);
}

bool TRPCHttpResponse::get_http_header(const std::string& name,
									   std::string& value) const
{
	protocol::HttpHeaderCursor cursor(this);
	return cursor.find(name, value);
}

} // namespace srpc

