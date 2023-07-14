/*
  Copyright (c) 2023 Sogou, Inc.

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

#include "rpc_trace_module.h"
#include "workflow/HttpUtil.h"

namespace srpc
{

static void generate_common_trace(SubTask *task, RPCModuleData& data)
{
	// Don't set start_timestamp here, which may mislead other modules in end()

	if (data.find(SRPC_TRACE_ID) == data.end())
	{
		uint64_t trace_id_high = SRPCGlobal::get_instance()->get_random();
		uint64_t trace_id_low = SRPCGlobal::get_instance()->get_random();
		std::string trace_id_buf(SRPC_TRACEID_SIZE + 1, 0);

		memcpy((char *)trace_id_buf.c_str(), &trace_id_high,
				SRPC_TRACEID_SIZE / 2);
		memcpy((char *)trace_id_buf.c_str() + SRPC_TRACEID_SIZE / 2,
				&trace_id_low, SRPC_TRACEID_SIZE / 2);

		data[SRPC_TRACE_ID] = std::move(trace_id_buf);
	}
	else
		data[SRPC_PARENT_SPAN_ID] = data[SRPC_SPAN_ID];

	uint64_t span_id = SRPCGlobal::get_instance()->get_random();
	std::string span_id_buf(SRPC_SPANID_SIZE + 1, 0);

	memcpy((char *)span_id_buf.c_str(), &span_id, SRPC_SPANID_SIZE);
	data[SRPC_SPAN_ID] = std::move(span_id_buf);
}

bool TraceModule::client_begin(SubTask *task, RPCModuleData& data)
{
	generate_common_trace(task, data);

	data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());
	data[SRPC_SPAN_KIND] = SRPC_SPAN_KIND_CLIENT;

	// clear other unnecessary module_data since the data comes from series
	data.erase(SRPC_DURATION);
	data.erase(SRPC_FINISH_TIMESTAMP);

	return true;
}

bool TraceModule::client_end(SubTask *task, RPCModuleData& data)
{
	// 1. failed to call any client_begin() previously
	if (data.find(SRPC_START_TIMESTAMP) == data.end())
	{
		generate_common_trace(task, data);
	}
	// 2. other modules has not set duration yet
	else if (data.find(SRPC_DURATION) == data.end())
	{
		unsigned long long end_time = GET_CURRENT_NS();
		data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
		data[SRPC_DURATION] = std::to_string(end_time -
							atoll(data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

bool TraceModule::server_begin(SubTask *task, RPCModuleData& data)
{
	generate_common_trace(task, data);

	data[SRPC_START_TIMESTAMP] = std::to_string(GET_CURRENT_NS());
	data[SRPC_SPAN_KIND] = SRPC_SPAN_KIND_SERVER;

	return true;
}

bool TraceModule::server_end(SubTask *task, RPCModuleData& data)
{
	// 1. failed to call any server_begin() previously
	if (data.find(SRPC_START_TIMESTAMP) == data.end())
	{
		generate_common_trace(task, data);
	}
	// 2. some other module has not set duration yet
	else if (data.find(SRPC_DURATION) == data.end())
	{
		unsigned long long end_time = GET_CURRENT_NS();

		data[SRPC_FINISH_TIMESTAMP] = std::to_string(end_time);
		data[SRPC_DURATION] = std::to_string(end_time -
							atoll(data[SRPC_START_TIMESTAMP].data()));
	}

	return true;
}

void TraceModule::client_begin_request(protocol::HttpRequest *req,
									   RPCModuleData& data) const
{
	data[SRPC_HTTP_METHOD] = req->get_method();

	const void *body;
	size_t body_len;
	req->get_parsed_body(&body, &body_len);
	data[SRPC_HTTP_REQ_LEN] = std::to_string(body_len);
}

void TraceModule::client_end_response(protocol::HttpResponse *resp,
									  RPCModuleData& data) const
{
	data[SRPC_HTTP_STATUS_CODE] = resp->get_status_code();

	const void *body;
	size_t body_len;
	resp->get_parsed_body(&body, &body_len);
	data[SRPC_HTTP_RESP_LEN] = std::to_string(body_len);
}

void TraceModule::server_begin_request(protocol::HttpRequest *req,
									   RPCModuleData& data) const
{
	data[SRPC_HTTP_METHOD] = req->get_method();
	data[SRPC_HTTP_TARGET] = req->get_request_uri();

	const void *body;
	size_t body_len;
	req->get_parsed_body(&body, &body_len);
	data[SRPC_HTTP_REQ_LEN] = std::to_string(body_len);

	std::string name;
	std::string value;
	protocol::HttpHeaderCursor req_cursor(req);
	int flag = 0;
	while (req_cursor.next(name, value) && flag != 3)
	{
		if (strcasecmp(name.data(), "Host") == 0)
		{
			data[SRPC_HTTP_HOST_NAME] = value;
			flag |= 1;
		}
		else if (strcasecmp(name.data(), "X-Forwarded-For") == 0)
		{
			data[SRPC_HTTP_CLIENT_IP] = value;
			flag |= (1 << 1);
		}
	}
}

void TraceModule::server_end_response(protocol::HttpResponse *resp,
									  RPCModuleData& data) const
{
	data[SRPC_HTTP_STATUS_CODE] = resp->get_status_code();
	data[SRPC_HTTP_RESP_LEN] = std::to_string(resp->get_output_body_size());
}

} // end namespace srpc

