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

#include <errno.h>
#include <vector>
#include <string>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <workflow/HttpUtil.h>
#include <workflow/StringUtil.h>
#include "rpc_basic.h"
#include "rpc_compress.h"
#include "rpc_meta.pb.h"
#include "rpc_message_srpc.h"
#include "rpc_zero_copy_stream.h"

namespace srpc
{

struct SRPCHttpHeadersString
{
	const std::string RPCCompressType	=	"Content-Encoding";
	const std::string OriginSize		=	"Origin-Size";
	const std::string CompressdSize		=	"Content-Length";
	const std::string DataType			=	"Content-Type";
	const std::string SRPCStatus		=	"SRPC-Status";
	const std::string SRPCError			=	"SRPC-Error";
};

struct CaseCmp
{
	bool operator()(const std::string& lhs, const std::string& rhs) const
	{
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

static const struct SRPCHttpHeadersString SRPCHttpHeaders;

static const std::map<const std::string, int, CaseCmp> SRPCHttpHeadersCode =
{
	{SRPCHttpHeaders.RPCCompressType,		1},
	{SRPCHttpHeaders.OriginSize,			2},
	{SRPCHttpHeaders.CompressdSize,		3},
	{SRPCHttpHeaders.DataType,				4},
	{SRPCHttpHeaders.SRPCStatus,			5},
	{SRPCHttpHeaders.SRPCError,			6}
};

static const std::vector<std::string> RPCDataTypeString =
{
	"application/x-protobuf",
	"application/x-thrift",
	"application/json"
};

static const std::vector<std::string> RPCRPCCompressTypeString =
{
	"identity",
	"x-snappy",
	"gzip",
	"deflate",
	"x-lz4"
};

static constexpr const char* kTypeUrlPrefix = "type.googleapis.com";

class ResolverInstance
{
public:
	static google::protobuf::util::TypeResolver* get_resolver()
	{
		static ResolverInstance kInstance;

		return kInstance.resolver_;
	}

private:
	ResolverInstance()
	{
		resolver_ = google::protobuf::util::NewTypeResolverForDescriptorPool(kTypeUrlPrefix,
									google::protobuf::DescriptorPool::generated_pool());
	}

	~ResolverInstance() { delete resolver_; }

	google::protobuf::util::TypeResolver* resolver_;
};

static inline std::string GetTypeUrl(const ProtobufIDLMessage *pb_msg)
{
	return std::string(kTypeUrlPrefix) + "/" + pb_msg->GetDescriptor()->full_name();
}

SRPCMessage::SRPCMessage()
{
	this->nreceived = 0;
	this->meta_buf = NULL;
	this->meta_len = 0;
	this->message_len = 0;
	memset(this->header, 0, sizeof (this->header));
	this->meta = new RPCMeta();
	this->buf = new RPCBuffer();
}

int SRPCMessage::append(const void *buf, size_t *size, size_t size_limit)
{
	uint32_t *p;
	size_t header_left, body_received, buf_len;

	if (this->nreceived < SRPC_HEADER_SIZE)
	{
		//receive header
		header_left = SRPC_HEADER_SIZE - this->nreceived;
		if (*size >= header_left)
		{
			//receive the whole header and ready to recieve body
			memcpy(this->header + this->nreceived, buf, header_left);
			this->nreceived += header_left;
			p = (uint32_t *)this->header + 1;
			this->meta_len = ntohl(*p);
			p = (uint32_t *)this->header + 2;
			this->message_len = ntohl(*p);
			buf_len = this->meta_len + this->message_len;

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

					this->buf->append((const char *)buf + header_left + this->meta_len,
									  *size - header_left - this->meta_len,
									  BUFFER_MODE_COPY);
				}

				this->nreceived += *size - header_left;
				if (this->nreceived == SRPC_HEADER_SIZE + buf_len)
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
		body_received = this->nreceived - SRPC_HEADER_SIZE;
		buf_len = this->meta_len + this->message_len;
		if (body_received + *size > buf_len)
			*size = buf_len - body_received;

		if (body_received + *size <= this->meta_len)
		{
			// 100 + 3 <= 106
			memcpy(this->meta_buf + body_received, buf, *size);
		} else if (body_received < this->meta_len) {
			// 100 + ? > 106, 100 < 106
			memcpy(this->meta_buf + body_received, buf,
				   this->meta_len - body_received);

			if (body_received + *size > this->meta_len)// useless. always true
				// 100 + 10 > 106
				this->buf->append((const char *)buf + this->meta_len - body_received,
								  *size - this->meta_len + body_received,
								  BUFFER_MODE_COPY);
		} else {
			// 110 > 106
			this->buf->append((const char *)buf, *size, BUFFER_MODE_COPY);
		}

		this->nreceived += *size;
		return this->nreceived == SRPC_HEADER_SIZE + buf_len;
	}
}

int SRPCMessage::get_compress_type() const
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	return meta->compress_type();
}

int SRPCMessage::get_data_type() const
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	return meta->data_type();
}

void SRPCMessage::set_compress_type(int type)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	meta->set_compress_type(type);
}

void SRPCMessage::set_data_type(int type)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	meta->set_data_type(type);
}

void SRPCMessage::set_attachment_nocopy(const char *attachment, size_t len)
{
	//TODO:
}

bool SRPCMessage::get_attachment_nocopy(const char **attachment, size_t *len) const
{
	//TODO:
	return false;
}

bool SRPCMessage::set_meta_module_data(const RPCModuleData& data)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	auto iter = data.find("trace_id");
	if (iter != data.end())
	{
		meta->mutable_span()->set_trace_id(iter->second.c_str(),
										   SRPC_TRACEID_SIZE);
	}

	iter = data.find("span_id");
	if (iter != data.end())
	{
		meta->mutable_span()->set_span_id(iter->second.c_str(),
										  SRPC_SPANID_SIZE);
	}
	//	meta->mutable_span()->set_parent_span_id(span->parent_span_id);
	//	name...
	return true;
}

bool SRPCMessage::get_meta_module_data(RPCModuleData& data) const
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);
	if (!meta->has_span())
		return false;

	data["trace_id"] = meta->mutable_span()->trace_id();
	data["span_id"] = meta->mutable_span()->span_id();

	if (meta->mutable_span()->has_parent_span_id())
		data["parent_span_id"] = meta->mutable_span()->parent_span_id();

	return true;
}

const std::string& SRPCRequest::get_service_name() const
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	return meta->mutable_request()->service_name();
}

const std::string& SRPCRequest::get_method_name() const
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	return meta->mutable_request()->method_name();
}

void SRPCRequest::set_service_name(const std::string& service_name)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	meta->mutable_request()->set_service_name(service_name);
}

void SRPCRequest::set_method_name(const std::string& method_name)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	meta->mutable_request()->set_method_name(method_name);
}

int SRPCResponse::get_status_code() const
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	return meta->mutable_response()->status_code();
}

void SRPCResponse::set_status_code(int code)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	meta->mutable_response()->set_status_code(code);
}

int SRPCResponse::get_error() const
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	return meta->mutable_response()->error();
}

const char *SRPCResponse::get_errmsg() const
{
	switch (this->get_status_code())
	{
	case RPCStatusOK:
		return "OK";
	case RPCStatusUndefined:
		return "Undefined Error";
	case RPCStatusServiceNotFound:
		return "Service Not Found";
	case RPCStatusMethodNotFound:
		return "Method Not Found";
	case RPCStatusMetaError:
		return "Meta Error";
	case RPCStatusReqCompressSizeInvalid:
		return "Request Compress-size Invalid";
	case RPCStatusReqDecompressSizeInvalid:
		return "Request Decompress-size Invalid";
	case RPCStatusReqCompressNotSupported:
		return "Request Compress Not Supported";
	case RPCStatusReqDecompressNotSupported:
		return "Request Decompress Not Supported";
	case RPCStatusReqCompressError:
		return "Request Compress Error";
	case RPCStatusReqDecompressError:
		return "Request Decompress Error";
	case RPCStatusReqSerializeError:
		return "Request Serialize Error";
	case RPCStatusReqDeserializeError:
		return "Request Deserialize Error";
	case RPCStatusRespCompressSizeInvalid:
		return "Response Compress-size Invalid";
	case RPCStatusRespDecompressSizeInvalid:
		return "Response Decompress-size Invalid";
	case RPCStatusRespCompressNotSupported:
		return "Response Compress Not Supported";
	case RPCStatusRespDecompressNotSupported:
		return "Response Decompress Not Supported";
	case RPCStatusRespCompressError:
		return "Response Compress Error";
	case RPCStatusRespDecompressError:
		return "Response Decompress Error";
	case RPCStatusRespSerializeError:
		return "Response Serialize Error";
	case RPCStatusRespDeserializeError:
		return "Response Deserialize Error";
	case RPCStatusIDLSerializeNotSupported:
		return "IDL Serialize Not Supported";
	case RPCStatusIDLDeserializeNotSupported:
		return "IDL Deserialize Not Supported";
	case RPCStatusURIInvalid:
		return "URI Invalid";
	case RPCStatusUpstreamFailed:
		return "Upstream Failed";
	case RPCStatusSystemError:
		return "System Error. Use get_error() to get errno";
	case RPCStatusSSLError:
		return "SSL Error. Use get_error() to get SSL-Error";
	case RPCStatusDNSError:
		return "DNS Error. Use get_error() to get GAI-Error";
	case RPCStatusProcessTerminated:
		return "Process Terminated";
	default:
		return "Unknown Error";
	}
}

void SRPCResponse::set_error(int error)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);

	meta->mutable_response()->set_error(error);
}

int SRPCMessage::serialize(const ProtobufIDLMessage *pb_msg)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int data_type = meta->data_type();
	int ret;

	if (!pb_msg)
		return RPCStatusOK;

	RPCOutputStream output_stream(this->buf, pb_msg->ByteSize());

	if (data_type == RPCDataProtobuf)
	{
		ret = pb_msg->SerializeToZeroCopyStream(&output_stream) ? 0 : -1;
		this->message_len = this->buf->size();
	}
	else if (data_type == RPCDataJson)
	{
		std::string binary_input = pb_msg->SerializeAsString();
		google::protobuf::io::ArrayInputStream input_stream(binary_input.data(), (int)binary_input.size());
		const auto* pool = pb_msg->GetDescriptor()->file()->pool();
		auto* resolver = (pool == google::protobuf::DescriptorPool::generated_pool()
							? ResolverInstance::get_resolver()
							: google::protobuf::util::NewTypeResolverForDescriptorPool(kTypeUrlPrefix, pool));

		ret = BinaryToJsonStream(resolver, GetTypeUrl(pb_msg), &input_stream, &output_stream).ok() ? 0 : -1;
		if (pool != google::protobuf::DescriptorPool::generated_pool())
			delete resolver;

		this->message_len = this->buf->size();
	}
	else
		ret = -1;

	if (ret < 0)
		return is_resp ? RPCStatusRespSerializeError : RPCStatusReqSerializeError;

	return RPCStatusOK;
}

int SRPCMessage::deserialize(ProtobufIDLMessage *pb_msg)
{
	const RPCMeta *meta = static_cast<const RPCMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int data_type = meta->data_type();
	int ret;
	RPCInputStream input_stream(this->buf);

	if (data_type == RPCDataProtobuf)
		ret = pb_msg->ParseFromZeroCopyStream(&input_stream) ? 0 : -1;
	else if (data_type == RPCDataJson)
	{
		std::string binary_output;
		google::protobuf::io::StringOutputStream output_stream(&binary_output);
		const auto* pool = pb_msg->GetDescriptor()->file()->pool();
		auto* resolver = (pool == google::protobuf::DescriptorPool::generated_pool()
							? ResolverInstance::get_resolver()
							: google::protobuf::util::NewTypeResolverForDescriptorPool(kTypeUrlPrefix, pool));

		if (JsonToBinaryStream(resolver, GetTypeUrl(pb_msg), &input_stream, &output_stream).ok())
			ret = pb_msg->ParseFromString(binary_output) ? 0 : -1;
		else
			ret = -1;

		if (pool != google::protobuf::DescriptorPool::generated_pool())
			delete resolver;
	}
	else
		ret = -1;

	if (ret < 0)
		return is_resp ? RPCStatusRespDeserializeError : RPCStatusReqDeserializeError;

	return RPCStatusOK;
}

int SRPCMessage::serialize(const ThriftIDLMessage *thrift_msg)
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int data_type = meta->data_type();
	int ret;

	if (!thrift_msg)
		return RPCStatusOK;

	ThriftBuffer thrift_buffer(this->buf);

	if (data_type == RPCDataThrift)
		ret = thrift_msg->descriptor->writer(thrift_msg, &thrift_buffer) ? 0 : -1;
	else if (data_type == RPCDataJson)
		ret = thrift_msg->descriptor->json_writer(thrift_msg, &thrift_buffer) ? 0 : -1;
	else
		ret = -1;

	if (ret < 0)
		return is_resp ? RPCStatusRespSerializeError : RPCStatusReqSerializeError;

	this->message_len = this->buf->size();
	return RPCStatusOK;
}

int SRPCMessage::deserialize(ThriftIDLMessage *thrift_msg)
{
	const RPCMeta *meta = static_cast<const RPCMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int data_type = meta->data_type();
	int ret;

	if (this->buf->size() == 0 || this->message_len == 0)
		return is_resp ? RPCStatusRespDeserializeError
					   : RPCStatusReqDeserializeError;

	ThriftBuffer thrift_buffer(this->buf);

	if (data_type == RPCDataThrift)
		ret = thrift_msg->descriptor->reader(&thrift_buffer, thrift_msg) ? 0 : 1;
	else if (data_type == RPCDataJson)
		ret = thrift_msg->descriptor->json_reader(&thrift_buffer, thrift_msg) ? 0 : 1;
	else
		ret = -1;

	if (ret < 0)
		return is_resp ? RPCStatusRespDeserializeError
					   : RPCStatusReqDeserializeError;

	return RPCStatusOK;
}

int SRPCMessage::compress()
{
	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int type = meta->compress_type();
	size_t buflen = this->message_len;
	int origin_size;
	int status_code = RPCStatusOK;

	if (buflen == 0)
	{
		if (type != RPCCompressNone)
		{
			meta->set_origin_size(0);
			meta->set_compressed_size(0);
		}
		return status_code;
	}

	if (type == RPCCompressNone)
		return status_code;

	if (buflen > 0x7FFFFFFF)
	{
		return is_resp ? RPCStatusRespCompressSizeInvalid
					   : RPCStatusReqCompressSizeInvalid;
	}

	origin_size = (int)buflen;
	static RPCCompressor *compressor = RPCCompressor::get_instance();
	int ret = compressor->lease_compressed_size(type, buflen);

	if (ret == -2)
	{
		return is_resp ? RPCStatusReqCompressNotSupported
					   : RPCStatusRespCompressNotSupported;
	}
	else if (ret <= 0)
	{
		return is_resp ? RPCStatusRespCompressSizeInvalid
					   : RPCStatusReqCompressSizeInvalid;
	}

	buflen = ret;
	if (this->buf->size() != this->message_len)
		return is_resp ? RPCStatusRespCompressError : RPCStatusReqCompressError;

	RPCBuffer *dst_buf = new RPCBuffer();
	ret = compressor->serialize_to_compressed(this->buf, dst_buf, type);

	if (ret == -2)
	{
		status_code = is_resp ? RPCStatusRespCompressNotSupported
							  : RPCStatusReqCompressNotSupported;
	}
	else if (ret == -1)
	{
		status_code = is_resp ? RPCStatusRespCompressError
							  : RPCStatusReqCompressError;
	}
	else if (ret <= 0)
	{
		status_code = is_resp ? RPCStatusRespCompressSizeInvalid
							  : RPCStatusReqCompressSizeInvalid;
	}
	else
	{
		meta->set_origin_size(origin_size);
		meta->set_compressed_size(ret);
		buflen = ret;
	}

	if (status_code == RPCStatusOK)
	{
		delete this->buf;
		this->buf = dst_buf;
		this->message_len = buflen;
	} else {
		delete dst_buf;
	}

	return status_code;
}

int SRPCMessage::decompress()
{
	const RPCMeta *meta = static_cast<const RPCMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int type = meta->compress_type();
	int status_code = RPCStatusOK;

	if (this->message_len == 0 || type == RPCCompressNone)
		return status_code;

	if (meta->compressed_size() == 0)
	{
		return is_resp ? RPCStatusRespDecompressSizeInvalid
					   : RPCStatusReqDecompressSizeInvalid;
	}

	if (this->buf->size() != (size_t)meta->compressed_size())
		return is_resp ? RPCStatusRespCompressError : RPCStatusReqCompressError;

	RPCBuffer *dst_buf = new RPCBuffer();
	static RPCCompressor *compressor = RPCCompressor::get_instance();
	int ret = compressor->parse_from_compressed(this->buf, dst_buf, type);

	if (ret == -2)
	{
		status_code = is_resp ? RPCStatusRespDecompressNotSupported
							  : RPCStatusReqDecompressNotSupported;
	}
	else if (ret == -1)
	{
		status_code = is_resp ? RPCStatusRespDecompressError
							  : RPCStatusReqDecompressError;
	}
	else if (ret <= 0 || (meta->has_origin_size() && ret != meta->origin_size()))
	{
		status_code = is_resp ? RPCStatusRespDecompressSizeInvalid
							  : RPCStatusReqDecompressSizeInvalid;
	}

	if (status_code == RPCStatusOK)
	{

		delete this->buf;
		this->buf = dst_buf;
		this->message_len = ret;
	} else {
		delete dst_buf;
	}

	return status_code;
}

static bool __deserialize_meta_http(const char *request_uri,
									protocol::HttpMessage *http_msg,
									SRPCMessage *srpc_msg,
									ProtobufIDLMessage *pb_msg)
{
	RPCMeta *meta = static_cast<RPCMeta *>(pb_msg);
	protocol::HttpHeaderCursor header_cursor(http_msg);
	std::string key, value;

	while (header_cursor.next(key, value))
	{
		const auto it = SRPCHttpHeadersCode.find(key);

		if (it != SRPCHttpHeadersCode.cend())
		{
			switch (it->second)
			{
			case 1:
				for (size_t i = 0; i < RPCRPCCompressTypeString.size(); i++)
				{
					if (strcasecmp(RPCRPCCompressTypeString[i].c_str(),
								   value.c_str()) == 0)
					{
						meta->set_compress_type(i);
						break;
					}
				}

				break;
			case 2:
				meta->set_origin_size(atoi(value.c_str()));
				break;
			case 4:
				for (size_t i = 0; i < RPCDataTypeString.size(); i++)
				{
					if (strcasecmp(RPCDataTypeString[i].c_str(),
								   value.c_str()) == 0)
					{
						meta->set_data_type(i);
						break;
					}
				}

				break;
			default:
				continue;
			}
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

		for (char& c : str)
		{
			if (c == '/')
				c = '.';
		}

		pos = str.find_last_of('.');
		if (pos != std::string::npos)
		{
			meta->mutable_request()->set_service_name(str.substr(0, pos));
			meta->mutable_request()->set_method_name(str.substr(pos + 1));
		}
	}

	const void *ptr;
	size_t len;

	http_msg->get_parsed_body(&ptr, &len);
	if (len > 0x7FFFFFFF)
		return false;

	protocol::HttpChunkCursor chunk_cursor(http_msg);
	RPCBuffer *buf = srpc_msg->get_buffer();
	size_t msg_len = 0;

	while (chunk_cursor.next(&ptr, &len))
	{
		buf->append((const char *)ptr, len, BUFFER_MODE_NOCOPY);
		msg_len += len;
	}

	srpc_msg->set_message_len(msg_len);

	if (!meta->has_data_type())
		meta->set_data_type(RPCDataJson);

	if (!meta->has_compress_type())
		meta->set_compress_type(RPCCompressNone);

	if (meta->compress_type() == RPCCompressNone)
	{
		if (msg_len == 0 && meta->data_type() == RPCDataJson)
		{
			buf->append("{}", 2, BUFFER_MODE_NOCOPY);
			srpc_msg->set_message_len(2);
		}
	}
	else
		meta->set_compressed_size(msg_len);

	return true;
}

bool SRPCHttpRequest::serialize_meta()
{
	if (this->buf->size() > 0x7FFFFFFF)
		return false;

	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);
	int data_type = meta->data_type();
	int compress_type = meta->compress_type();

	set_http_version("HTTP/1.1");
	set_method("POST");
	set_request_uri("/" + meta->mutable_request()->service_name() +
					"/" + meta->mutable_request()->method_name());

	//set header
	set_header_pair(SRPCHttpHeaders.DataType,
					RPCDataTypeString[data_type]);

	set_header_pair(SRPCHttpHeaders.RPCCompressType,
					RPCRPCCompressTypeString[compress_type]);

	if (compress_type != RPCCompressNone)
	{
		set_header_pair(SRPCHttpHeaders.CompressdSize,
						std::to_string(meta->compressed_size()));

		set_header_pair(SRPCHttpHeaders.OriginSize,
						std::to_string(meta->origin_size()));
	} else {
		set_header_pair("Content-Length", std::to_string(this->message_len));
	}

	set_header_pair("Connection", "Keep-Alive");

	const void *buffer;
	size_t buflen;

	//append body
	while (buflen = this->buf->fetch(&buffer), buffer && buflen > 0)
		this->append_output_body_nocopy(buffer, buflen);

	return true;
}

bool SRPCHttpRequest::deserialize_meta()
{
	const char *request_uri = this->get_request_uri();
	return __deserialize_meta_http(request_uri, this, this, this->meta);
}

bool SRPCHttpResponse::serialize_meta()
{
	if (this->buf->size() > 0x7FFFFFFF)
		return false;

	RPCMeta *meta = static_cast<RPCMeta *>(this->meta);
	int data_type = meta->data_type();
	int compress_type = meta->compress_type();
	int rpc_status_code = this->get_status_code();

	set_http_version("HTTP/1.1");
	if (rpc_status_code == RPCStatusOK)
		protocol::HttpUtil::set_response_status(this, HttpStatusOK);
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
		protocol::HttpUtil::set_response_status(this, HttpStatusServiceUnavailable);
	else
		protocol::HttpUtil::set_response_status(this, HttpStatusInternalServerError);

	//set header
	set_header_pair(SRPCHttpHeaders.SRPCStatus,
					std::to_string(meta->mutable_response()->status_code()));

	set_header_pair(SRPCHttpHeaders.SRPCError,
					std::to_string(meta->mutable_response()->error()));

	set_header_pair(SRPCHttpHeaders.DataType,
					RPCDataTypeString[data_type]);

	set_header_pair(SRPCHttpHeaders.RPCCompressType,
					RPCRPCCompressTypeString[compress_type]);

	if (compress_type != RPCCompressNone)
	{
		set_header_pair(SRPCHttpHeaders.CompressdSize,
						std::to_string(meta->compressed_size()));

		set_header_pair(SRPCHttpHeaders.OriginSize,
						std::to_string(meta->origin_size()));
	} else {
		set_header_pair("Content-Length", std::to_string(this->message_len));
	}

	set_header_pair("Connection", "Keep-Alive");

	const void *buffer;
	size_t buflen;

	//append body
	while (buflen = this->buf->fetch(&buffer), buffer && buflen > 0)
		this->append_output_body_nocopy(buffer, buflen);

	return true;
}

bool SRPCHttpResponse::deserialize_meta()
{
	return __deserialize_meta_http(NULL, this, this, this->meta);
}

bool SRPCHttpRequest::set_meta_module_data(const RPCModuleData& data)
{
	auto iter = data.find("trace_id");
	if (iter != data.end())
		set_header_pair("Trace-Id", iter->second);

	iter = data.find("span_id");
	if (iter != data.end())
		set_header_pair("Span-Id", iter->second);

	return true;
}

bool SRPCHttpRequest::get_meta_module_data(RPCModuleData& data) const
{
	std::string name;
	std::string value;

	protocol::HttpHeaderCursor cursor(this);
	bool found = false;

	while (cursor.next(name, value))
	{
		if (!strcasecmp(name.c_str(), "Trace-Id"))
		{
			//span->set_trace_id(strtoll(value.c_str(), NULL, 10));
			data["trace_id"] = value;
			found = true;
			continue;
		}

		if (!strcasecmp(name.c_str(), "Span-Id"))
		{
			//span->set_parent_span_id(strtoll(value.c_str(), NULL, 10));
			data["parent_span_id"] = value;
			found = true;
			continue;
		}
	}
	return found;
}

bool SRPCHttpResponse::set_meta_module_data(const RPCModuleData& data)
{
	auto iter = data.find("trace_id");
	if (iter != data.end())
		set_header_pair("Trace-Id", iter->second);

	iter = data.find("span_id");
	if (iter != data.end())
		set_header_pair("Span-Id", iter->second);

	return true;
}

bool SRPCHttpResponse::get_meta_module_data(RPCModuleData& data) const
{
	std::string name;
	std::string value;

	protocol::HttpHeaderCursor cursor(this);
	bool found = false;

	while (cursor.next(name, value))
	{
		if (!strcasecmp(name.c_str(), "Trace-Id"))
		{
			//span->set_trace_id(strtoll(value.c_str(), NULL, 10));
			data["trace_id"] = value;
			found = true;
			continue;
		}

		if (!strcasecmp(name.c_str(), "Span-Id"))
		{
			//span->set_parent_span_id(strtoll(value.c_str(), NULL, 10));
			data["parent_span_id"] = value;
			found = true;
			continue;
		}
	}
	return found;
}

} // namespace srpc

