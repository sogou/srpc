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

#include "rpc_message_trpc.h"
#include "rpc_meta_trpc.pb.h"
#include "rpc_basic.h"
#include "rpc_compress.h"
#include "rpc_zero_copy_stream.h"

namespace srpc
{

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

	this->srpc_status_code = RPCStatusOK;

	if (meta->ret() != TrpcRetCode::TRPC_INVOKE_SUCCESS)
	{
		this->srpc_status_code = meta->ret();
		this->srpc_error_msg = meta->error_msg();
	}

	return true;
}

bool TRPCRequest::serialize_meta()
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);
	meta->set_content_type(TrpcContentEncodeType::TRPC_PROTO_ENCODE);
	meta->set_version(TrpcProtoVersion::TRPC_PROTO_V1);
	meta->set_call_type(TrpcCallType::TRPC_UNARY_CALL);

	this->meta_len = meta->ByteSize();
	this->meta_buf = new char[this->meta_len];
	return this->meta->SerializeToArray(this->meta_buf, (int)this->meta_len);
}

bool TRPCResponse::serialize_meta()
{
	ResponseProtocol *meta = static_cast<ResponseProtocol *>(this->meta);
	meta->set_version(TrpcProtoVersion::TRPC_PROTO_V1);
	meta->set_call_type(TrpcCallType::TRPC_UNARY_CALL);

	this->meta_len = meta->ByteSize();
	this->meta_buf = new char[this->meta_len];
	if (this->srpc_status_code != RPCStatusOK)
	{
		meta->set_ret(this->status_code_srpc_trpc(this->srpc_status_code)); //TODO
		meta->set_error_msg(this->error_msg_srpc_trpc(this->srpc_status_code));
		// this->srpc_error_msg
	}

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

int TRPCMessage::status_code_srpc_trpc(int srpc_status_code) const
{
	return 0;//TODO
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

	ResponseProtocol *meta = dynamic_cast<ResponseProtocol *>(this->meta);
	bool is_resp = (meta != NULL);

	int msg_len = pb_msg->ByteSize();
	RPCOutputStream stream(this->message, pb_msg->ByteSize());
	int ret = pb_msg->SerializeToZeroCopyStream(&stream) ? 0 : -1;

	if (ret < 0)
		return is_resp ? RPCStatusRespSerializeError : RPCStatusReqSerializeError;

	this->message_len = msg_len;
	return RPCStatusOK;
}

int TRPCMessage::deserialize(ProtobufIDLMessage *pb_msg)
{
	ResponseProtocol *meta = dynamic_cast<ResponseProtocol *>(this->meta);
	bool is_resp = (meta != NULL);

	RPCInputStream stream(this->message);

	if (pb_msg->ParseFromZeroCopyStream(&stream) == false)
		return is_resp ? RPCStatusRespDeserializeError : RPCStatusReqDeserializeError;

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

bool TRPCRequest::set_meta_module_data(const RPCModuleData& data)
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	for (auto & pair : data)
		meta->mutable_trans_info()->insert({pair.first, pair.second});

	return true;
}

bool TRPCRequest::get_meta_module_data(RPCModuleData& data) const
{
	RequestProtocol *meta = static_cast<RequestProtocol *>(this->meta);

	for (auto & pair : meta->trans_info())
		data.insert(pair);

	return true;
}

bool TRPCRequest::trim_service_prefix()
{
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

} // namespace srpc

