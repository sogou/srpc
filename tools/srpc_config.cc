/*
  Copyright (c) 2022 Sogou, Inc.

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include "srpc_config.h"

std::vector<std::string> RPC_PROTOC_SKIP_FILES =
{ "config_simple.h", "config_simple.cc",
  "rpc.thrift", "client_thrift.cc", "server_thrift.cc" };

std::vector<std::string> RPC_THRIFT_SKIP_FILES =
{ "config_simple.h", "config_simple.cc",
  "rpc.proto", "client_protobuf.cc", "server_protobuf.cc" };

void usage_kafka(int argc, const char *argv[])
{
	printf("Usage:\n"
		   "    %s kafka <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Flags:\n"
		   "    -o :    project output path (default: CURRENT_PATH)\n"
		   "    -h :    kafka broker url (example: kafka://10.160.23.23:9000,"
		   "10.123.23.23,kafka://kafka.sogou)\n"
		   "    -g :    group name\n"
		   "    -d :    path of dependencies (default: COMPILE_PATH)\n"
		   , argv[0]);
}

const char *get_type_string(uint8_t type)
{
	switch (type)
	{
	case PROTOCOL_TYPE_HTTP:
		return "Http";
	case PROTOCOL_TYPE_REDIS:
		return "Redis";
	case PROTOCOL_TYPE_MYSQL:
		return "MySQL";
	case PROTOCOL_TYPE_KAFKA:
		return "Kafka";
	case PROTOCOL_TYPE_SRPC:
		return "SRPC";
	case PROTOCOL_TYPE_SRPC_HTTP:
		return "SRPCHttp";
	case PROTOCOL_TYPE_BRPC:
		return "BRPC";
	case PROTOCOL_TYPE_THRIFT:
		return "Thrift";
	case PROTOCOL_TYPE_THRIFT_HTTP:
		return "ThriftHttp";
	case PROTOCOL_TYPE_TRPC:
		return "TRPC";
	case PROTOCOL_TYPE_TRPC_HTTP:
		return "TRPCHttp";
	default:
		return "Unknown type";
	}
}

static int check_file_idl_type(const char *filename)
{
	size_t len = strlen(filename);

	if (len > 6 && strcmp(filename + len - 6, ".proto") == 0)
		return IDL_TYPE_PROTOBUF;
	else if (len > 7 && strcmp(filename + len - 7, ".thrift") == 0)
		return IDL_TYPE_THRIFT;

	return IDL_TYPE_MAX;
}

srpc_config::srpc_config()
{
	project_name = NULL;
	service_name = NULL;
	rpc_type = PROTOCOL_TYPE_SRPC;
	idl_type = IDL_TYPE_DEFAULT;
	data_type = DATA_TYPE_DEFAULT;
	compress_type = COMPRESS_TYPE_NONE;
	specified_depend_path = false;
	specified_idl_file = NULL;
	specified_idl_path = NULL;
}

bool srpc_config::prepare_specified_idl_file()
{
	if (this->specified_idl_file == NULL)
	{
		if (this->specified_idl_path != NULL)
		{
			printf(COLOR_RED"Error:\n      idl_path is specified "
				   "but NO idl_file.\n\n" COLOR_OFF);
			return false;
		}

		return true;
	}
	else if (this->service_name != NULL)
	{
		printf(COLOR_RED"Error:\n      [-s service_name] does NOT take effect "
			   "when idl_file is specified.\n\n" COLOR_OFF);
		return false;
	}

	this->idl_type = check_file_idl_type(this->specified_idl_file);
	if (this->idl_type == IDL_TYPE_MAX)
	{
		printf(COLOR_RED"Error:\n      Invalid idl type. file :" COLOR_BLUE
			   " %s\n\n" COLOR_OFF, this->specified_idl_file);
		return false;
	}

	if (access(this->specified_idl_file, F_OK) != 0)
	{
		printf(COLOR_RED"Error:\n      idl_file" COLOR_BLUE " %s " COLOR_RED
			   "does NOT exist.\n\n" COLOR_OFF, this->specified_idl_file);
		return false;
	}

	if (this->specified_idl_path == NULL)
		this->specified_idl_path = this->output_path;

	return true;
}

const char *srpc_config::rpc_type_string() const
{
	return get_type_string(this->rpc_type);
}

const char *srpc_config::rpc_compress_string() const
{
	switch (this->compress_type)
	{
	case COMPRESS_TYPE_SNAPPY:
		return "RPCCompressSnappy";
	case COMPRESS_TYPE_GZIP:
		return "RPCCompressGzip";
	case COMPRESS_TYPE_ZLIB:
		return "RPCCompressZlib";
	case COMPRESS_TYPE_LZ4:
		return "RPCCompressLz4";
	default:
		return "Unknown type";
	}
}

const char *srpc_config::rpc_data_string() const
{
	switch (this->data_type)
	{
	case DATA_TYPE_PROTOBUF:
		return "RPCDataProtobuf";
	case DATA_TYPE_THRIFT:
		return "RPCDataThrift";
	case DATA_TYPE_JSON:
		return "RPCDataJson";
	default:
		return "Unknown type";
	}
}

void srpc_config::set_rpc_type(const char *type)
{
	if (strcasecmp(type, "SRPCHttp") == 0)
		this->rpc_type = PROTOCOL_TYPE_SRPC_HTTP;
	else if (strcasecmp(type, "SRPC") == 0)
		this->rpc_type = PROTOCOL_TYPE_SRPC;
	else if (strcasecmp(type, "BRPC") == 0)
		this->rpc_type = PROTOCOL_TYPE_BRPC;
	else if (strcasecmp(type, "ThriftHttp") == 0)
		this->rpc_type = PROTOCOL_TYPE_THRIFT_HTTP;
	else if (strcasecmp(type, "Thrift") == 0)
		this->rpc_type = PROTOCOL_TYPE_THRIFT;
	else if (strcasecmp(type, "TRPCHttp") == 0)
		this->rpc_type = PROTOCOL_TYPE_TRPC_HTTP;
	else if (strcasecmp(type, "TRPC") == 0)
		this->rpc_type = PROTOCOL_TYPE_TRPC;
	else
		this->rpc_type = PROTOCOL_TYPE_MAX;
}

void srpc_config::set_idl_type(const char *type)
{
	if (strcasecmp(type, "protobuf") == 0)
		this->idl_type = IDL_TYPE_PROTOBUF;
	else if (strcasecmp(type, "thrift") == 0)
		this->idl_type = IDL_TYPE_THRIFT;
	else
		this->idl_type = IDL_TYPE_MAX;
}

void srpc_config::set_data_type(const char *type)
{
	if (strcasecmp(type, "protobuf") == 0)
		this->data_type = DATA_TYPE_PROTOBUF;
	else if (strcasecmp(type, "thrift") == 0)
		this->data_type = DATA_TYPE_THRIFT;
	else if (strcasecmp(type, "json") == 0)
		this->data_type = DATA_TYPE_JSON;
	else
		this->data_type = DATA_TYPE_MAX;
}

void srpc_config::set_compress_type(const char *type)
{
	if (strcasecmp(type, "snappy") == 0)
		this->compress_type = COMPRESS_TYPE_SNAPPY;
	else if (strcasecmp(type, "gzip") == 0)
		this->compress_type = COMPRESS_TYPE_GZIP;
	else if (strcasecmp(type, "zlib") == 0)
		this->compress_type = COMPRESS_TYPE_ZLIB;
	else if (strcasecmp(type, "lz4") == 0)
		this->compress_type = COMPRESS_TYPE_LZ4;
	else
		this->compress_type = COMPRESS_TYPE_MAX;
}

const char *srpc_config::proxy_server_type_string() const
{
	return get_type_string(this->proxy_server_type);
}

const char *srpc_config::proxy_client_type_string() const
{
	return get_type_string(this->proxy_client_type);
}

int check_proxy_type(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP ||
		type == PROTOCOL_TYPE_REDIS ||
		type == PROTOCOL_TYPE_MYSQL ||
		type == PROTOCOL_TYPE_KAFKA)
		return PROXY_BASIC_TYPE;

	if (type == PROTOCOL_TYPE_SRPC ||
		type == PROTOCOL_TYPE_SRPC_HTTP ||
		type == PROTOCOL_TYPE_BRPC ||
		type == PROTOCOL_TYPE_TRPC ||
		type == PROTOCOL_TYPE_TRPC_HTTP)
		return PROXY_PROTOBUF_TYPE;

	if (type == PROTOCOL_TYPE_THRIFT ||
		type == PROTOCOL_TYPE_THRIFT_HTTP)
		return PROXY_THRIFT_TYPE;

	return -1;
}

ControlGenerator::ControlGenerator(const struct srpc_config *config) :
	Generator(config->idl_type == IDL_TYPE_THRIFT ? true : false),
	ctl_printer(config->idl_type == IDL_TYPE_THRIFT ? true : false),
	config(config)
{ }

bool ControlGenerator::generate_client_cpp_file(const idl_info& info,
												const std::string& idl_file)
{
	this->client_cpp_file = this->config->output_path;
	this->client_cpp_file += "client_main.cc";

	if (this->ctl_printer.open(this->client_cpp_file) == false)
		return false;

	this->ctl_printer.print_clt_include();
	this->ctl_printer.print_client_file_include(idl_file);

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		for (const auto& rpc : desc.rpcs)
		{
			this->ctl_printer.print_client_done_method_ctl(info.package_name,
														   desc.block_name,
														   rpc.method_name,
														   rpc.response_name);
		}
	}

	this->ctl_printer.print_client_main_begin();
	this->ctl_printer.print_client_load_config();
	this->ctl_printer.print_client_params();

	int id = 0;

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		std::string suffix;
		if (id != 0)
			suffix = std::to_string(id);
		id++;

		this->ctl_printer.print_client_main_service_ctl(this->config->rpc_type_string(),
														info.package_name,
														desc.block_name,
														suffix);

		auto rpc_it = desc.rpcs.cbegin();
		if (rpc_it != desc.rpcs.cend())
		{
			this->ctl_printer.print_client_main_method_call(info.package_name,
															desc.block_name,
															rpc_it->method_name,
															rpc_it->request_name,
															suffix);
			rpc_it++;
		}
	}

	this->ctl_printer.print_client_main_end();
	this->ctl_printer.close();

	return false;
}

bool ControlGenerator::generate_server_cpp_file(const idl_info& info,
												const std::string& idl_file)
{
	this->server_cpp_file = this->config->output_path;
	this->server_cpp_file += "server_main.cc";

	if (this->ctl_printer.open(this->server_cpp_file) == false)
		return false;

	this->ctl_printer.print_clt_include();
	this->ctl_printer.print_server_file_include(idl_file); 

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		this->ctl_printer.print_server_class_impl(info.package_name,
												  desc.block_name);

		for (const auto& rpc : desc.rpcs)
		{
			this->ctl_printer.print_server_impl_method(info.package_name,
													   desc.block_name,
													   rpc.method_name,
													   rpc.request_name,
													   rpc.response_name);
		}
		this->ctl_printer.print_server_class_impl_end();
	}

	this->ctl_printer.print_server_main_begin();
	this->ctl_printer.print_server_load_config();
	this->ctl_printer.print_server_construct(this->config->rpc_type_string());

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		this->ctl_printer.print_server_main_method(desc.block_name);
	}

	this->ctl_printer.print_server_main_end_ctl(this->config->project_name,
											this->config->rpc_type_string());
	this->ctl_printer.print_server_main_return();
	this->ctl_printer.close();

	return true;	
}

static std::string ctl_include_format = R"(#include <stdio.h>
#include "config/config.h"
)";

static std::string ctl_load_config_format = R"(
	// load config
	srpc::RPCConfig config;
	if (config.load("./%s") == false)
	{
		perror("Load config failed");
		exit(1);
	}

	signal(SIGINT, sig_handler);
)";

static std::string ctl_client_load_params_format = R"(
	// start client
	RPCClientParams params = RPC_CLIENT_PARAMS_DEFAULT;
	params.host = config.client_host();
	params.port = config.client_port();
	params.transport_type = config.client_transport_type();
	params.is_ssl = config.client_is_ssl();
	params.url = config.client_url();
	params.callee_timeout = config.client_callee_timeout();
	params.caller = config.client_caller();

	params.task_params.retry_max = config.client_retry_max();
)";

static std::string ctl_client_main_params_format = R"(
	%s::%sClient client%s(&params);
	config.load_filter(client%s);
)";

static std::string ctl_client_done_protobuf_format = R"(
		// printf("%%s\n", response->DebugString().c_str());
)";

static std::string ctl_client_done_format = R"(
static void %s_done(%s *response, srpc::RPCContext *context)
{
	if (context->success())
	{
		// TODO: fill your logic to set response%s
	}
	else
		printf("%s %s status[%%d] error[%%d] errmsg:%%s\n",
			   context->get_status_code(), context->get_error(),
			   context->get_errmsg());
}
)";

static std::string ctl_server_main_end_format = R"(
	config.load_filter(server);

	if (server.start(config.server_port()) == 0)
	{
		printf("%s %s server started, port %%u\n", config.server_port());
		wait_group.wait();
		server.stop();
	}
	else
		perror("server start");
)";

void ControlGenerator::ControlPrinter::print_clt_include()
{
	fprintf(this->out_file, "%s", ctl_include_format.c_str());
}

void ControlGenerator::ControlPrinter::print_client_load_config()
{
	fprintf(this->out_file, ctl_load_config_format.c_str(), "./client.conf");
}

void ControlGenerator::ControlPrinter::print_server_load_config()
{
	fprintf(this->out_file, ctl_load_config_format.c_str(), "./server.conf");
}

void ControlGenerator::ControlPrinter::print_client_params()
{
	fprintf(this->out_file, "%s", ctl_client_load_params_format.c_str());
	// TODO: set client task params
}

void ControlGenerator::ControlPrinter::
print_client_done_method_ctl(const std::vector<std::string>& package,
							 const std::string& service,
							 const std::string& method,
							 const std::string& response)
{
	std::string method_lower = method;
	std::transform(method_lower.begin(), method_lower.end(),
				   method_lower.begin(), ::tolower);

	std::string resp;
	const char *set_resp_code = "";

	if (this->is_thrift)
		resp = make_thrift_package_prefix(package, service, response);
	else
		resp = make_package_prefix(package, response);

	if (!this->is_thrift)
		set_resp_code = ctl_client_done_protobuf_format.c_str();

	fprintf(this->out_file, ctl_client_done_format.c_str(),
			method_lower.c_str(), resp.c_str(), set_resp_code,
			service.c_str(), method.c_str(), service.c_str(), method.c_str());
}

void ControlGenerator::ControlPrinter::
print_client_main_service_ctl(const std::string& type,
							  const std::vector<std::string>& package,
							  const std::string& service,
							  const std::string& suffix)
{
	std::string base_service = make_package_prefix(package, service);
	fprintf(this->out_file, ctl_client_main_params_format.c_str(),
			base_service.c_str(), type.c_str(),
			suffix.c_str(), suffix.c_str());
}

void ControlGenerator::ControlPrinter::
print_server_construct(const char *rpc_type)
{
	fprintf(this->out_file, "    %sServer server;\n", rpc_type);
}

void ControlGenerator::ControlPrinter::
print_server_main_end_ctl(const char *project_name, const char *rpc_type)
{
	fprintf(this->out_file, ctl_server_main_end_format.c_str(),
			project_name, rpc_type);
}

