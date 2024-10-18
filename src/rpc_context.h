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

#ifndef __RPC_CONTEXT_H__
#define __RPC_CONTEXT_H__
#include <string>
#include <functional>
#include <workflow/Workflow.h>
#include "rpc_basic.h"

namespace srpc
{

struct RPCSyncContext
{
	long long seqid;
	std::string errmsg;
	std::string remote_ip;
	int status_code;
	int error;
	bool success;
	int timeout_reason;
};

class RPCContext
{
public:
	virtual long long get_seqid() const = 0;
	virtual std::string get_remote_ip() const = 0;
	virtual int get_peer_addr(struct sockaddr *addr, socklen_t *addrlen) const = 0;
	virtual const std::string& get_service_name() const = 0;
	virtual const std::string& get_method_name() const = 0;

	virtual SeriesWork *get_series() const = 0;
	virtual bool get_http_header(const std::string& name,
								 std::string& value) const = 0;

public:
	// for client-done
	virtual bool success() const = 0;
	virtual int get_status_code() const = 0;
	virtual const char *get_errmsg() const = 0;
	virtual int get_error() const = 0;
	virtual void *get_user_data() const = 0;
	virtual int get_timeout_reason() const = 0;

public:
	// for server-process
	virtual void set_data_type(RPCDataType type) = 0;//enum RPCDataType
	virtual void set_compress_type(RPCCompressType type) = 0;//enum RPCCompressType
	virtual void set_attachment_nocopy(const char *attachment, size_t len) = 0;
	virtual bool get_attachment(const char **attachment, size_t *len) const = 0;

	virtual void set_reply_callback(std::function<void (RPCContext *ctx)> cb) = 0;
	virtual void set_send_timeout(int timeout) = 0;
	virtual void set_keep_alive(int timeout) = 0;
	virtual bool set_http_code(int code) = 0;
	virtual bool set_http_header(const std::string& name, const std::string& value) = 0;
	virtual bool add_http_header(const std::string& name, const std::string& value) = 0;

	virtual bool log(const RPCLogVector& fields) = 0;

	// Refer to : https://opentelemetry.io/docs/reference/specification/baggage/api
	// corresponding to SetValue(), GetValue(), RemoveValue()
	virtual bool add_baggage(const std::string& key, const std::string& value) = 0;
	virtual bool get_baggage(const std::string& key, std::string& value) = 0;
	//virtual bool remove_baggage(const std::string& key) = 0;

	//virtual void noreply();
	//virtual WFConnection *get_connection();

public:
	// for json format
	// Currently only support pb to json. Default : false

	// Whether to add spaces, line breaks and indentation to make the JSON
	// output easy to read.
	virtual void set_json_add_whitespace(bool on) = 0;

	// Whether to always print enums as ints.
	virtual void set_json_always_print_enums_as_ints(bool flag) = 0;

	// Whether to preserve proto field names.
	virtual void set_json_preserve_proto_field_names(bool flag) = 0;

	// Whether to always print primitive fields / with no presence.
	// By default proto3 primitive fields with default values will be omitted
	// in JSON output. For example, an int32 field set to 0 will be omitted.
	// Set this flag to true will override the default behavior and print
	// primitive fields regardless of their values.
	virtual void set_json_always_print_fields_with_no_presence(bool flag) = 0;

	// deprecated : Please use set_json_always_print_fields_with_no_presence()
	void set_json_always_print_primitive_fields(bool flag)
	{
		this->set_json_always_print_fields_with_no_presence(flag);
	}

public:
	virtual ~RPCContext() { }
};

} // namespace srpc

////////
// inl
#include "rpc_context.inl"
#include "rpc_task.inl"

#endif

