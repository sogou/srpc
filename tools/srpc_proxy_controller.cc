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
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unordered_map>

#include "srpc_controller.h"

enum
{
	BASIC_TYPE = 1,
	PROTOBUF_TYPE = 2,
	THRIFT_TYPE = 3
};

static std::string default_server_port(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
		return "8080";
	if (type == PROTOCOL_TYPE_REDIS)
		return "6379";
	if (type == PROTOCOL_TYPE_MYSQL)
		return "3306";
	// Add other protocol here
	return "1412";
}

static std::string default_proxy_port(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
		return "8888";
	if (type == PROTOCOL_TYPE_REDIS)
		return "6378";
	if (type == PROTOCOL_TYPE_MYSQL)
		return "3305";
	// Add other protocol here
	return "1411";
}

static int check_proxy_type(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP ||
		type == PROTOCOL_TYPE_REDIS ||
		type == PROTOCOL_TYPE_MYSQL ||
		type == PROTOCOL_TYPE_KAFKA)
		return BASIC_TYPE;

	if (type == PROTOCOL_TYPE_SRPC ||
		type == PROTOCOL_TYPE_SRPC_HTTP ||
		type == PROTOCOL_TYPE_BRPC ||
		type == PROTOCOL_TYPE_TRPC ||
		type == PROTOCOL_TYPE_TRPC_HTTP)
		return PROTOBUF_TYPE;

	if (type == PROTOCOL_TYPE_THRIFT ||
		type == PROTOCOL_TYPE_THRIFT_HTTP)
		return THRIFT_TYPE;

	return -1;
}

static std::string proxy_process_request_codes(uint8_t server_type,
											   uint8_t client_type)
{
	if (server_type == client_type)
		return std::string(R"(
    *client_task->get_req() = std::move(*req);
)");
	else
		return std::string(R"(    {
        // TODO: fill the client request to server request
    }
)");
}

static std::string proxy_callback_response_codes(uint8_t server_type,
												 uint8_t client_type)
{
	if (server_type != client_type)
		return std::string(R"(
    {
        // TODO: fill the server response to client response
    }
)");

	if (server_type == PROTOCOL_TYPE_HTTP)
		return std::string(R"(
    {
        const void *body;
        size_t len;

        resp->get_parsed_body(&body, &len);
        resp->append_output_body_nocopy(body, len);
        *proxy_resp = std::move(*resp);
    }
    else
    {
        proxy_resp->set_status_code("404");
        proxy_resp->append_output_body_nocopy(
                            "<html>404 Not Found.</html>", 27);
    }
)");

	return std::string(R"(
        *proxy_resp = std::move(*resp);
)");
}

static std::string proxy_redirect_codes(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
		return std::string(R"(
                                                              config.redirect_max(),)");

	return std::string("");
}

static bool proxy_basic_transform(const std::string& format, FILE *out,
								  const struct srpc_config *config)
{
	const char *server_type = config->proxy_server_type_string();
	const char *client_type = config->proxy_client_type_string();
	std::string server_lower = server_type;
	std::transform(server_lower.begin(), server_lower.end(),
				   server_lower.begin(), ::tolower);
	std::string server_port = default_server_port(config->proxy_server_type);
	std::string proxy_port = default_proxy_port(config->proxy_client_type);

	size_t len = fprintf(out, format.c_str(), client_type, server_type,
						 server_type, client_type, client_type,
						 proxy_callback_response_codes(config->proxy_server_type,
													   config->proxy_client_type).c_str(),
						 // process
						 client_type, client_type, server_lower.c_str(),
						 server_type, server_lower.c_str(),
						 proxy_redirect_codes(config->proxy_server_type).c_str(),
						 proxy_process_request_codes(config->proxy_server_type,
													 config->proxy_client_type).c_str(),
						 // main
						 client_type, server_type, client_type);

	return len > 0;
}

static bool proxy_config_transform(const std::string& format, FILE *out,
								   const struct srpc_config *config)
{
	std::string server_port = default_server_port(config->proxy_server_type);
	std::string proxy_port = default_proxy_port(config->proxy_client_type);

	size_t len = fprintf(out, format.c_str(),
						 proxy_port.c_str(), server_port.c_str());

	return len > 0;
}

static bool proxy_rpc_proxy_transform(const std::string& format, FILE *out,
									  const struct srpc_config *config)
{
	const char *server_type = config->proxy_server_type_string();
	const char *client_type = config->proxy_client_type_string();

	size_t len = fprintf(out, format.c_str(),
						 config->project_name, // not support specified idl file
						 config->project_name, config->project_name,
						 config->project_name, server_type,
						 // main
						 client_type, config->project_name,
						 client_type, server_type);

	return len > 0;
}

ProxyController::ProxyController()
{
	this->config.type = COMMAND_PROXY;
	this->config.proxy_client_type = PROTOCOL_TYPE_HTTP;
	this->config.proxy_server_type = PROTOCOL_TYPE_HTTP;

	struct file_info info;

	info = { "proxy/proxy.conf", "proxy.conf", proxy_config_transform };
	this->default_files.push_back(info);

	info = { "basic/server.conf", "server.conf", basic_server_config_transform };
	this->default_files.push_back(info);

	info = { "basic/client.conf", "client.conf", basic_client_config_transform };
	this->default_files.push_back(info);

	info = { "common/GNUmakefile", "GNUmakefile", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.h", "config/Json.h", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.cc", "config/Json.cc", nullptr };
	this->default_files.push_back(info);
}

void ProxyController::print_usage(const char *name) const
{
	printf(COLOR_PINK"Usage:\n"
		   COLOR_INFO"    %s " COLOR_BLUE "proxy "
		   COLOR_INFO "<PROJECT_NAME>" COLOR_FLAG " [FLAGS]\n\n"
		   COLOR_PINK"Available Flags:\n"
		   COLOR_FLAG"    -c :"
		   COLOR_WHITE" client type for proxy [ Http | Redis | SRPC | SRPCHttp"
		   " | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)\n"
		   COLOR_FLAG"    -s :"
		   COLOR_WHITE" server type for proxy [ Http | Redis | SRPC | SRPCHttp"
		   " | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)\n"
		   COLOR_FLAG"    -o :"
		   COLOR_WHITE" project output path (default: CURRENT_PATH)\n"
		   COLOR_FLAG"    -d :"
		   COLOR_WHITE" path of dependencies (default: COMPILE_PATH)\n"
		   COLOR_OFF, name);
}

void ProxyController::print_success_info() const
{
	printf(COLOR_GREEN"Success:\n      make project path "
		   COLOR_BLUE"\" %s \"" COLOR_GREEN " done.\n\n" COLOR_OFF,
		   this->config.output_path);
	printf(COLOR_PINK"Commands:\n"
		   COLOR_BLUE "      cd %s\n      make -j\n\n" COLOR_OFF,
		   this->config.output_path);
	printf(COLOR_PINK"Execute:\n"
		   COLOR_GREEN"      ./server\n      ./proxy\n      ./client\n\n" COLOR_OFF);
}

bool ProxyController::copy_files()
{
	struct file_info info;

	if (check_proxy_type(this->config.proxy_client_type) == BASIC_TYPE &&
		check_proxy_type(this->config.proxy_server_type) == BASIC_TYPE)
	{
		info = { "common/CMakeLists.txt", "CMakeLists.txt", common_cmake_transform };
		this->default_files.push_back(info);

		info = { "common/util.h", "config/util.h", nullptr };
		this->default_files.push_back(info);

		info = { "proxy/proxy_main.cc", "proxy_main.cc", proxy_basic_transform };
		this->default_files.push_back(info);

		info = { "basic/client_main.cc", "client_main.cc", basic_client_transform };
		this->default_files.push_back(info);

		info = { "basic/server_main.cc", "server_main.cc", basic_server_transform };
		this->default_files.push_back(info);

		info = { "config/config_simple.h", "config/config.h", nullptr };
		this->default_files.push_back(info);

		info = { "config/config_simple.cc", "config/config.cc", nullptr };
		this->default_files.push_back(info);
	}
	else
	{
		std::string proxy_main = "proxy/proxy_main_";
		if (this->config.idl_type == IDL_TYPE_PROTOBUF)
			proxy_main += "proto.cc";
		else
			proxy_main += "thrift.cc";

		info = { std::move(proxy_main), "proxy_main.cc", proxy_rpc_proxy_transform };
		this->default_files.push_back(info);

		info = { "rpc/CMakeLists.txt", "CMakeLists.txt", rpc_cmake_transform };
		this->default_files.push_back(info);

		info = { "config/config_full.h", "config/config.h", nullptr };
		this->default_files.push_back(info);

		info = { "config/config_full.cc", "config/config.cc", nullptr };
		this->default_files.push_back(info);

		if (this->config.specified_idl_file == NULL)
			this->fill_rpc_default_files();
		else
			return false; // TODO: NOT supported yet
	}

	return CommandController::copy_files();
}

static uint8_t proxy_string_to_type(const char *type)
{
	if (strcasecmp(type, "http") == 0)
		return PROTOCOL_TYPE_HTTP;
	else if (strcasecmp(type, "redis") == 0)
		return PROTOCOL_TYPE_REDIS;
	else if (strcasecmp(type, "mysql") == 0)
		return PROTOCOL_TYPE_MYSQL;
	else if (strcasecmp(type, "kafka") == 0)
		return PROTOCOL_TYPE_KAFKA;
	else if (strcasecmp(type, "SRPC") == 0)
		return PROTOCOL_TYPE_SRPC;
	else if (strcasecmp(type, "SRPCHttp") == 0)
		return PROTOCOL_TYPE_SRPC_HTTP;
	else if (strcasecmp(type, "BRPC") == 0)
		return PROTOCOL_TYPE_BRPC;
	else if (strcasecmp(type, "TRPC") == 0)
		return PROTOCOL_TYPE_TRPC;
	else if (strcasecmp(type, "TRPCHttp") == 0)
		return PROTOCOL_TYPE_TRPC_HTTP;
	else if (strcasecmp(type, "Thrift") == 0)
		return PROTOCOL_TYPE_THRIFT;
	else if (strcasecmp(type, "ThriftHTTP") == 0)
		return PROTOCOL_TYPE_THRIFT_HTTP;

	return PROTOCOL_TYPE_MAX;
}

bool ProxyController::get_opt(int argc, const char **argv)
{
	struct srpc_config *config = &this->config;
	char c;
	optind = 3;

	while ((c = getopt(argc, (char * const *)argv, "o:c:s:d:")) != -1)
	{
		switch (c)
		{
		case 'o':
			if (sscanf(optarg, "%s", config->output_path) != 1)
				return false;
			break;
		case 'c':
			config->proxy_client_type = proxy_string_to_type(optarg);
			break;
		case 's':
			config->proxy_server_type = proxy_string_to_type(optarg);
			break;
		case 'd':
			config->specified_depend_path = true;
			memset(config->depend_path, 0, MAXPATHLEN);
			if (sscanf(optarg, "%s", config->depend_path) != 1)
				return false;
			break;
		default:
			printf(COLOR_RED"Error:\n     Unknown args : "
				   COLOR_BLUE"%s\n\n" COLOR_OFF, argv[optind - 1]);
			return false;
		}
	}

	return true;
}

bool ProxyController::check_args()
{
	int server_type = check_proxy_type(this->config.proxy_server_type);
	int client_type = check_proxy_type(this->config.proxy_client_type);

	if (CommandController::check_args() == false)
		return false;

	if (client_type < 0 || server_type < 0)
	{
		printf(COLOR_RED"Error:\n     Invalid type :"
			   COLOR_BLUE" %s" COLOR_RED ", " COLOR_BLUE "%s\n\n" COLOR_OFF,
			   this->config.proxy_server_type_string(),
			   this->config.proxy_client_type_string());
		return false;
	}

	if ((client_type == BASIC_TYPE && server_type > BASIC_TYPE) ||
		(server_type == BASIC_TYPE && client_type > BASIC_TYPE) ||
		// TODO: temperarily only support workflow to workflow, rpc to rpc
		(client_type == PROTOBUF_TYPE && server_type == THRIFT_TYPE) ||
		(server_type == PROTOBUF_TYPE && client_type == THRIFT_TYPE))
		// TODO: temperarily NOT support protobuf with thrift
	{
		printf(COLOR_RED"Error:\n     Temperarily not support "
			   COLOR_BLUE"%s" COLOR_RED " and " COLOR_BLUE "%s"
			   COLOR_RED" together\n\n" COLOR_OFF,
			   this->config.proxy_server_type_string(),
			   this->config.proxy_client_type_string());
		return false;
	}

	if (client_type == PROTOBUF_TYPE)
	{
		this->config.idl_type = IDL_TYPE_PROTOBUF;
	}
	else if (client_type == THRIFT_TYPE)
	{
		// this->config.idl_type = IDL_TYPE_THRIFT;
		printf(COLOR_RED"Error:\n     Temperarily not support IDL thrift.\n\n" COLOR_OFF);
		return false;
	}

	return true;
}

