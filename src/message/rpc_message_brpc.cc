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
#include <workflow/HttpUtil.h>
#include <workflow/StringUtil.h>
#include "rpc_basic.h"
#include "rpc_compress.h"
#include "rpc_meta_brpc.pb.h"
#include "rpc_message_brpc.h"
#include "rpc_zero_copy_stream.h"

namespace srpc
{

BRPCMessage::BRPCMessage()
{
	this->nreceived = 0;
	this->meta_buf = NULL;
	this->meta_len = 0;
	this->message_len = 0;
	this->attachment_len = 0;
	memset(this->header, 0, sizeof (this->header));
	this->meta = new BrpcMeta();
	this->message = new RPCBuffer();
	this->attachment = NULL;
}

bool BRPCRequest::deserialize_meta()
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	if (meta->ParseFromArray(this->meta_buf, (int)this->meta_len))
	{
		if (meta->has_attachment_size())
		{
			this->attachment_len = meta->attachment_size();
			this->message_len -= this->attachment_len;
			this->attachment = new RPCBuffer();
			this->message->cut(this->message_len, this->attachment);
		}

		return true;
	}

	return false;
}

bool BRPCResponse::deserialize_meta()
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	if (meta->ParseFromArray(this->meta_buf, (int)this->meta_len))
	{
		if (meta->has_attachment_size())
		{
			this->attachment_len = meta->attachment_size();
			this->message_len -= this->attachment_len;
			this->attachment = new RPCBuffer();
			this->message->cut(this->message_len, this->attachment);
		}

		this->srpc_status_code = RPCStatusOK;
		if (meta->has_response())
		{
			if (meta->mutable_response()->error_code() != 0)
			{
				this->srpc_status_code = RPCStatusMetaError;
				this->srpc_error_msg = meta->mutable_response()->error_text();
			}
		}

		return true;
	}

	return false;
}

int BRPCMessage::append(const void *buf, size_t *size, size_t size_limit)
{
	uint32_t *p;
	size_t header_left, body_received, buf_len;

	if (this->nreceived < BRPC_HEADER_SIZE)
	{
		//receive header
		header_left = BRPC_HEADER_SIZE - this->nreceived;
		if (*size >= header_left)
		{
			//receive the whole header and ready to recieve body
			memcpy(this->header + this->nreceived, buf, header_left);
			this->nreceived += header_left;
			p = (uint32_t *)this->header + 1;
			buf_len = ntohl(*p); // payload_len
			p = (uint32_t *)this->header + 2;
			this->meta_len = ntohl(*p);
			this->message_len = buf_len - this->meta_len; // msg_len + attachment_len

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
//				this->buf = new char[this->message_len];

				if (*size - header_left <= this->meta_len)
				{
					memcpy(this->meta_buf, (const char *)buf + header_left,
						   *size - header_left);
				}
				else
				{
					memcpy(this->meta_buf, (const char *)buf + header_left,
						   this->meta_len);
//					memcpy(this->buf,
//						   (const char *)buf + header_left + this->meta_len,
//						   *size - header_left - this->meta_len);
					this->message->append((const char *)buf + header_left + this->meta_len,
										  *size - header_left - this->meta_len,
										  BUFFER_MODE_COPY);
				}

				this->nreceived += *size - header_left;
				if (this->nreceived == BRPC_HEADER_SIZE + buf_len)
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
		body_received = this->nreceived - BRPC_HEADER_SIZE;
		buf_len = this->meta_len + this->message_len;
		if (body_received + *size > buf_len)
			*size = buf_len - body_received;

		if (body_received + *size <= this->meta_len)
		{
			memcpy(this->meta_buf + body_received, buf, *size);
		}
		else if (body_received < this->meta_len)
		{
			memcpy(this->meta_buf + body_received, buf,
				   this->meta_len - body_received);

			if (body_received + *size > this->meta_len)// useless. always true
//				memcpy(this->buf, (const char *)buf + this->meta_len - body_received,
//					   *size - this->meta_len + body_received);
				this->message->append((const char *)buf + this->meta_len - body_received,
									  *size - this->meta_len + body_received,
									  BUFFER_MODE_COPY);
		}
		else
		{
//			memcpy(this->buf + body_received - this->meta_len, buf, *size);
			this->message->append((const char *)buf, *size, BUFFER_MODE_COPY);
		}

		this->nreceived += *size;
		return this->nreceived == BRPC_HEADER_SIZE + buf_len;
	}
}

bool BRPCRequest::serialize_meta()
{
	this->meta_len = meta->ByteSize();
	this->meta_buf = new char[this->meta_len];
	return this->meta->SerializeToArray(this->meta_buf, (int)this->meta_len);
}

bool BRPCResponse::serialize_meta()
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	this->meta_len = meta->ByteSize();
	this->meta_buf = new char[this->meta_len];
	if (this->srpc_status_code != RPCStatusOK)
	{
		meta->mutable_response()->set_error_code(2001); //TODO
		meta->mutable_response()->set_error_text(this->srpc_error_msg);
	}

	return meta->SerializeToArray(this->meta_buf, (int)this->meta_len);
}

int BRPCMessage::get_compress_type() const
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	return meta->compress_type();
}

void BRPCMessage::set_compress_type(int type)
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	meta->set_compress_type(type);
}

void BRPCMessage::set_attachment_nocopy(const char *attachment, size_t len)
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	this->attachment_len += len;
	meta->set_attachment_size(this->attachment_len);
	this->attachment = new RPCBuffer();
	this->attachment->append(attachment, len, BUFFER_MODE_NOCOPY);
}

bool BRPCMessage::get_attachment_nocopy(const char **attachment, size_t *len) const
{
	size_t tmp_len = *len;
	const void *tmp_buf;

	if (this->attachment == NULL ||
		this->attachment->fetch(&tmp_buf, &tmp_len) == false)
	{
		return false;
	}

	*attachment = (const char *)tmp_buf;
	*len = tmp_len;
	return true;
}

const std::string& BRPCRequest::get_service_name() const
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	return meta->mutable_request()->service_name();
}

const std::string& BRPCRequest::get_method_name() const
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	return meta->mutable_request()->method_name();
}

void BRPCRequest::set_service_name(const std::string& service_name)
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	meta->mutable_request()->set_service_name(service_name);
}

void BRPCRequest::set_method_name(const std::string& method_name)
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	meta->mutable_request()->set_method_name(method_name);
}

int64_t BRPCRequest::get_correlation_id() const
{
	const BrpcMeta *meta = static_cast<const BrpcMeta *>(this->meta);

	if (meta->has_correlation_id())
		return meta->correlation_id();

	return -1;
}

int BRPCResponse::get_status_code() const
{
	return this->srpc_status_code;
}

void BRPCResponse::set_status_code(int code)
{
	this->srpc_status_code = code;
	if (code != RPCStatusOK)
		this->srpc_error_msg = this->get_errmsg();
}

int BRPCResponse::get_error() const
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	return meta->mutable_response()->error_code();
}

const char *BRPCResponse::get_errmsg() const
{
	switch (this->srpc_status_code)
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

void BRPCResponse::set_error(int error)
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	meta->mutable_response()->set_error_code(error);
}

void BRPCResponse::set_correlation_id(int64_t cid)
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);

	meta->set_correlation_id(cid);
}

int BRPCMessage::serialize(const ProtobufIDLMessage *pb_msg)
{
	if (!pb_msg)
		return RPCStatusOK;

	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int msg_len = pb_msg->ByteSize();
	RPCOutputStream stream(this->message, pb_msg->ByteSize());
	int ret = pb_msg->SerializeToZeroCopyStream(&stream) ? 0 : -1;

	if (ret < 0)
		return is_resp ? RPCStatusRespSerializeError : RPCStatusReqSerializeError;

	this->message_len = msg_len;
	return RPCStatusOK;
}

int BRPCMessage::deserialize(ProtobufIDLMessage *pb_msg)
{
	const BrpcMeta *meta = static_cast<const BrpcMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	RPCInputStream stream(this->message);

	if (pb_msg->ParseFromZeroCopyStream(&stream) == false)
		return is_resp ? RPCStatusRespDeserializeError : RPCStatusReqDeserializeError;

	return RPCStatusOK;
}

int BRPCMessage::compress()
{
	BrpcMeta *meta = static_cast<BrpcMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int type = meta->compress_type();
	size_t buflen = this->message_len;
	int status_code = RPCStatusOK;

	if (buflen == 0)
		return status_code;

	if (type == RPCCompressNone)
		return status_code;

	static RPCCompressor *compressor = RPCCompressor::get_instance();
	int ret = compressor->lease_compressed_size(type, buflen);

	if (ret == -2)
		return is_resp ? RPCStatusReqCompressNotSupported : RPCStatusRespCompressNotSupported;
	else if (ret <= 0)
		return is_resp ? RPCStatusRespCompressSizeInvalid : RPCStatusReqCompressSizeInvalid;

	//buflen = ret;
	RPCBuffer *dst_buf = new RPCBuffer();
	ret = compressor->serialize_to_compressed(this->message, dst_buf, type);

	if (ret == -2)
		status_code = is_resp ? RPCStatusRespCompressNotSupported : RPCStatusReqCompressNotSupported;
	else if (ret == -1)
		status_code = is_resp ? RPCStatusRespCompressError : RPCStatusReqCompressError;
	else if (ret <= 0)
		status_code = is_resp ? RPCStatusRespCompressSizeInvalid : RPCStatusReqCompressSizeInvalid;
	else
		buflen = ret;

	if (status_code == RPCStatusOK)
	{
		delete this->message;
		this->message = dst_buf;
		this->message_len = buflen;
	} else {
		delete dst_buf;
	}

	return status_code;
}

int BRPCMessage::decompress()
{
	const BrpcMeta *meta = static_cast<const BrpcMeta *>(this->meta);
	bool is_resp = !meta->has_request();
	int type = meta->compress_type();
	int status_code = RPCStatusOK;

	if (this->message_len == 0 || type == RPCCompressNone)
		return status_code;

	RPCBuffer *dst_buf = new RPCBuffer();
	static RPCCompressor *compressor = RPCCompressor::get_instance();
	int ret = compressor->parse_from_compressed(this->message, dst_buf, type);

	if (ret == -2)
		status_code = is_resp ? RPCStatusRespDecompressNotSupported : RPCStatusReqDecompressNotSupported;
	else if (ret == -1)
		status_code = is_resp ? RPCStatusRespDecompressError : RPCStatusReqDecompressError;
	else if (ret <= 0)
		status_code = is_resp ? RPCStatusRespDecompressSizeInvalid : RPCStatusReqDecompressSizeInvalid;

	if (status_code == RPCStatusOK)
	{
		delete this->message;
		this->message = dst_buf;
		this->message_len = ret;
	} else {
		delete dst_buf;
	}

	return status_code;
}

} // namesapce sogou 
