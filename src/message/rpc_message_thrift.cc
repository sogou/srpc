/*
  Copyright (c) 2020 sogou, Inc.

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
#include <workflow/HttpUtil.h>
#include "rpc_message_thrift.h"

namespace srpc
{

static int thrift_parser_append_message(const void *buf, size_t *size, ThriftBuffer *TBuffer)
{
	if (TBuffer->status == THRIFT_PARSE_END)
	{
		*size = 0;
		return 1;
	}

/*
	if (TBuffer->status == THRIFT_PARSE_INIT)
		TBuffer->status = THRIFT_GET_FRAME_SIZE;
*/

	if (TBuffer->status == THRIFT_GET_FRAME_SIZE)
	{
		size_t framesize_bytelen = sizeof (TBuffer->framesize);
		char *readbuf = (char*)&TBuffer->framesize;
		size_t read_size = 0;
		for (size_t i = 0; i < *size; i++)
		{
			read_size++;
			((char *)readbuf)[TBuffer->framesize_read_byte] = ((char *)buf)[i];
			if (++TBuffer->framesize_read_byte == framesize_bytelen)
			{
				TBuffer->status = THRIFT_GET_DATA;
				TBuffer->framesize = ntohl(TBuffer->framesize);
				if (TBuffer->framesize < 0)
				{
					errno = EBADMSG;
					return -1;
				}

				//TBuffer->readbuf = new char[TBuffer->framesize];
				break;
			}
		}

		size_t left_size = *size - read_size;

		*size = read_size;
		if (left_size == 0)
		{
			if (TBuffer->status == THRIFT_GET_DATA && TBuffer->framesize == 0)
			{
				TBuffer->status = THRIFT_PARSE_END;
				return 1;
			}
			else
				return 0;
		}
		else
		{
			int ret = thrift_parser_append_message((char *)buf + read_size, &left_size, TBuffer);

			*size += left_size;
			return ret;
		}
	}
	else if (TBuffer->status == THRIFT_GET_DATA)
	{
		size_t read_size = *size;

		if (TBuffer->readbuf_size + *size > (size_t)TBuffer->framesize)
			read_size = TBuffer->framesize - TBuffer->readbuf_size;

		//memcpy(TBuffer->readbuf + TBuffer->readbuf_size, (const char *)buf, read_size);
		TBuffer->buffer->append((const char *)buf, read_size, BUFFER_MODE_COPY);
		TBuffer->readbuf_size += read_size;
		*size = read_size;
		if (TBuffer->readbuf_size < (size_t)TBuffer->framesize)
			return 0;
		else if (TBuffer->readbuf_size == (uint32_t)TBuffer->framesize)
		{
			TBuffer->status = THRIFT_PARSE_END;
			return 1;
		}
	}

	errno = EBADMSG;
	return -1;
}

int ThriftMessage::append(const void *buf, size_t *size, size_t size_limit)
{
	return thrift_parser_append_message(buf, size, &TBuffer_);
}

bool ThriftResponse::serialize_meta()
{
	if (status_code_ == RPCStatusOK)
		TBuffer_.meta.message_type = TMT_REPLY;
	else
	{
		ThriftException ex;

		ex.type = (status_code_ == RPCStatusMethodNotFound ? TET_UNKNOWN_METHOD
														   : TET_UNKNOWN);
		ex.message = errmsg_;
		ex.descriptor->writer(&ex, &TBuffer_);
		TBuffer_.meta.message_type = TMT_EXCEPTION;
	}

	return TBuffer_.writeMessageBegin();
}

const char *thrift_error2errmsg(int error) 
{
	switch (error)
	{
	case TET_UNKNOWN:
		return "TApplicationException: Unknown application exception";
	case TET_UNKNOWN_METHOD:
		return "TApplicationException: Unknown method";
	case TET_INVALID_MESSAGE_TYPE:
		return "TApplicationException: Invalid message type";
	case TET_WRONG_METHOD_NAME:
		return "TApplicationException: Wrong method name";
	case TET_BAD_SEQUENCE_ID:
		return "TApplicationException: Bad sequence identifier";
	case TET_MISSING_RESULT:
		return "TApplicationException: Missing result";
	case TET_INTERNAL_ERROR:
		return "TApplicationException: Internal error";
	case TET_PROTOCOL_ERROR:
		return "TApplicationException: Protocol error";
	case TET_INVALID_TRANSFORM:
		return "TApplicationException: Invalid transform";
	case TET_INVALID_PROTOCOL:
		return "TApplicationException: Invalid protocol";
	case TET_UNSUPPORTED_CLIENT_TYPE:
		return "TApplicationException: Unsupported client type";
	default:
		return "TApplicationException: (Invalid exception type)";
	};
}

bool ThriftResponse::deserialize_meta()
{
	if (TBuffer_.readMessageBegin())
	{
		if (TBuffer_.meta.message_type == TMT_EXCEPTION)
		{
			ThriftException ex;

			if (ex.descriptor->reader(&TBuffer_, &ex))
			{
				status_code_ = (ex.type == TET_UNKNOWN_METHOD ? RPCStatusMethodNotFound
															  : RPCStatusMetaError);
				error_ = ex.type;
				errmsg_ = ex.message;
			}
			else
			{
				status_code_ = RPCStatusMetaError;
				error_ = TET_INTERNAL_ERROR;
				errmsg_ = thrift_error2errmsg(error_);
			}
		}

		return true;
	}

	return false;
}

const char *ThriftResponse::get_errmsg() const
{
	if (!errmsg_.empty())
		return errmsg_.c_str();
	return thrift_error2errmsg(error_);
}

bool ThriftHttpRequest::serialize_meta()
{
	if (buf_.size() > 0x7FFFFFFF)
		return false;

	if (!this->ThriftRequest::serialize_meta())
		return false;

	set_http_version("HTTP/1.1");
	set_method("POST");
	set_request_uri("/");

	set_header_pair("Connection", "Keep-Alive");
	set_header_pair("Content-Type", "application/x-thrift");
	set_header_pair("Content-Length", std::to_string(TBuffer_.meta.writebuf.size() + buf_.size()));

	this->append_output_body_nocopy(TBuffer_.meta.writebuf.c_str(), TBuffer_.meta.writebuf.size());
	const void *buf;
	size_t buflen;

	while (buflen = buf_.fetch(&buf), buf && buflen > 0)
		this->append_output_body_nocopy(buf, buflen);

	return true;
}

bool ThriftHttpRequest::deserialize_meta()
{
	const void *body;
	size_t body_len;

	get_parsed_body(&body, &body_len);
	if (body_len > 0x7FFFFFFF)
		return false;

	buf_.append((const char *)body, body_len, BUFFER_MODE_NOCOPY);
	TBuffer_.framesize = (int32_t)body_len;
	return this->ThriftRequest::deserialize_meta();
}

bool ThriftHttpResponse::serialize_meta()
{
	if (buf_.size() > 0x7FFFFFFF)
		return false;

	if (!this->ThriftResponse::serialize_meta())
		return false;

	int rpc_status_code = this->get_status_code();

	set_http_version("HTTP/1.1");
	if (rpc_status_code == RPCStatusOK)
		protocol::HttpUtil::set_response_status(this, HttpStatusOK);
	else if (rpc_status_code == RPCStatusServiceNotFound
			|| rpc_status_code == RPCStatusMethodNotFound
			|| rpc_status_code == RPCStatusMetaError
			|| rpc_status_code == RPCStatusURIInvalid)
		protocol::HttpUtil::set_response_status(this, HttpStatusBadRequest);
	else if (rpc_status_code == RPCStatusRespCompressNotSupported
			|| rpc_status_code == RPCStatusRespDecompressNotSupported
			|| rpc_status_code == RPCStatusIDLSerializeNotSupported
			|| rpc_status_code == RPCStatusIDLDeserializeNotSupported)
		protocol::HttpUtil::set_response_status(this, HttpStatusNotImplemented);
	else if (rpc_status_code == RPCStatusUpstreamFailed)
		protocol::HttpUtil::set_response_status(this, HttpStatusServiceUnavailable);
	else
		protocol::HttpUtil::set_response_status(this, HttpStatusInternalServerError);

	set_header_pair("Connection", "Keep-Alive");
	set_header_pair("Content-Type", "application/x-thrift");
	set_header_pair("Content-Length", std::to_string(TBuffer_.meta.writebuf.size() + buf_.size()));

	this->append_output_body_nocopy(TBuffer_.meta.writebuf.c_str(), TBuffer_.meta.writebuf.size());
	const void *buf;
	size_t buflen;

	while (buflen = buf_.fetch(&buf), buf && buflen > 0)
		this->append_output_body_nocopy(buf, buflen);

	return true;
}

bool ThriftHttpResponse::deserialize_meta()
{
	const void *body;
	size_t body_len;

	get_parsed_body(&body, &body_len);
	if (body_len > 0x7FFFFFFF)
		return false;

	buf_.append((const char *)body, body_len, BUFFER_MODE_NOCOPY);
	TBuffer_.framesize = (int32_t)body_len;
	return this->ThriftResponse::deserialize_meta();
}

}

