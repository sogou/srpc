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
	PROTOCOL_TYPE = 2,
	THRIFT_TYPE = 3
};

static std::string default_server_port(const std::string& type)
{
	if (type.compare("Http") == 0)
		return "8080";
	if (type.compare("Redis") == 0)
		return "6379";
	// Add other protocol here
	return "1412";
}

static std::string default_proxy_port(const std::string& type)
{
	if (type.compare("Http") == 0)
		return "8888";
	if (type.compare("Redis") == 0)
		return "6378";
	// Add other protocol here
	return "1411";
}

static std::string client_redirect_codes(const std::string& type)
{
	if (type.compare("Http") == 0)
		return std::string(R"(
                                                        2, // redirect
)");

	return std::string("\n");
}

static std::string client_task_callback_codes(const std::string& type)
{
	if (type.compare("Http") == 0)
		return std::string(R"(	
        if (state == WFT_STATE_SUCCESS) // print server response body
        {
            const void *body;
            size_t body_len;

            task->get_resp()->get_parsed_body(&body, &body_len);
            fwrite(body, 1, body_len, stdout);
            fflush(stdout);
        }
)");

	if (type.compare("Redis") == 0)
		return std::string(R"(
        protocol::RedisResponse *resp = task->get_resp();
        protocol::RedisValue val;

        if (state == WFT_STATE_SUCCESS && resp->parse_success() == true)
        {
            resp->get_result(val);
            fprintf(stderr, "response: %s\n", val.string_value().c_str());
        }
)");

	return std::string("Unknown type");
}

static std::string client_set_request_codes(const std::string& type)
{
	if (type.compare("Http") == 0)
		return std::string(R"(
    protocol::HttpRequest *req = task->get_req();
    req->set_request_uri("/client_request"); // will send to server by proxy
)");

	if (type.compare("Redis") == 0)
		return std::string(R"(
    task->get_req()->set_request("SET", {"k1", "v1"});
)");

	return std::string("Unknown type");
}

static std::string server_process_codes(const std::string& type)
{
	if (type.compare("Http") == 0)
		return std::string(R"(
        fprintf(stderr, "http server get request_uri: %s\n",
               task->get_req()->get_request_uri());
        print_peer_address<WFHttpTask>(task);

        task->get_resp()->append_output_body("<html>Hello from server!</html>");
)");

	if (type.compare("Redis") == 0)
		return std::string(R"(
        protocol::RedisRequest *req   = task->get_req();
        protocol::RedisResponse *resp = task->get_resp();
        protocol::RedisValue val;
        std::string cmd;

        if (req->parse_success() == false || req->get_command(cmd) == false)
            return;

        fprintf(stderr, "redis server get cmd: [%s] from ", cmd.c_str());
        print_peer_address<WFRedisTask>(task);

        val.set_status("OK"); // example: return OK to every requests
        resp->set_result(val);
)");

	return std::string("Unknown type");
}
static std::string proxy_process_request_codes(const std::string& server_type,
											   const std::string& client_type)
{
	if (server_type.compare(client_type) == 0)
		return std::string(R"(
    *client_task->get_req() = std::move(*req);
)");
	else
		return std::string(R"(    {
        // TODO: fill the client request to server request
    }
)");
}

static std::string proxy_callback_response_codes(const std::string& server_type,
												 const std::string& client_type)
{
	if (server_type.compare(client_type) != 0)
		return std::string(R"(
    {
        // fill the server response to client response
    }
)");

	if (server_type.compare("Http") == 0)
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

static std::string proxy_redirect_codes(const std::string& type)
{
	if (type.compare("Http") == 0)
		return std::string(R"(
                                                              config.redirect_max(),)");

	return std::string("");
}

static bool is_workflow_protocol(const char *type)
{
	if (strncasecmp(type, "Http",  strlen("Http")) == 0 ||
		strncasecmp(type, "Redis", strlen("Redis")) == 0)
		return true;

	return false;
}

static bool proxy_proxy_transform(const std::string& format, FILE *out,
								  const struct srpc_config *config)
{
	std::string server_type = config->proxy_server_type_string();
	std::string client_type = config->proxy_client_type_string();
	std::string server_lower = server_type;
	std::transform(server_lower.begin(), server_lower.end(),
				   server_lower.begin(), ::tolower);
	std::string server_port = default_server_port(server_type);
	std::string proxy_port = default_proxy_port(client_type);

	size_t len = fprintf(out, format.c_str(), client_type.c_str(),
						 server_type.c_str(), server_type.c_str(),
						 client_type.c_str(), client_type.c_str(),
						 proxy_callback_response_codes(server_type, client_type).c_str(),
						 // process
						 client_type.c_str(), client_type.c_str(),
						 server_lower.c_str(),
						 server_type.c_str(), server_lower.c_str(),
						 proxy_redirect_codes(server_type).c_str(),
						 proxy_process_request_codes(server_type, client_type).c_str(),
						 client_type.c_str(),
						 // main
						 client_type.c_str(), server_type.c_str(),
						 client_type.c_str());

	return len > 0;
}

static bool proxy_client_transform(const std::string& format, FILE *out,
								   const struct srpc_config *config)
{
	std::string client_type = config->proxy_client_type_string();
	std::string client_lower = client_type;
	std::transform(client_lower.begin(), client_lower.end(),
				   client_lower.begin(), ::tolower);
	std::string proxy_port = default_proxy_port(client_type);

	size_t len = fprintf(out, format.c_str(),
						 client_type.c_str(), client_lower.c_str(),
						 proxy_port.c_str(),
						 client_type.c_str(), client_lower.c_str(),
						 client_redirect_codes(client_type).c_str(),
						 client_type.c_str(), client_type.c_str(),
						 client_task_callback_codes(client_type).c_str(),
						 client_set_request_codes(client_type).c_str());

	return len > 0;
}

static bool proxy_server_transform(const std::string& format, FILE *out,
								   const struct srpc_config *config)
{
	std::string server_type = config->proxy_server_type_string();
	std::string server_port = default_server_port(server_type);

	size_t len = fprintf(out, format.c_str(),
						 server_type.c_str(), server_port.c_str(),
						 server_type.c_str(), server_type.c_str(),
						 server_process_codes(server_type).c_str(),
						 server_type.c_str());

	return len > 0;
}

static bool proxy_config_transform(const std::string& format, FILE *out,
								   const struct srpc_config *config)
{
	std::string server_port = default_server_port(config->proxy_server_type_string());
	std::string proxy_port = default_proxy_port(config->proxy_client_type_string());

	size_t len = fprintf(out, format.c_str(),
						 proxy_port.c_str(), server_port.c_str());

	return len > 0;
}

static bool proxy_rpc_proxy_transform(const std::string& format, FILE *out,
									  const struct srpc_config *config)
{
	std::string server_type = config->proxy_server_type_string();
	std::string client_type = config->proxy_client_type_string();

	size_t len = fprintf(out, format.c_str(),
						 config->project_name, // not support specified idl file
						 config->project_name, config->project_name,
						 config->project_name, server_type.c_str(),
						 // main
						 client_type.c_str(), config->project_name,
						 client_type.c_str(), server_type.c_str());

	return len > 0;
}

ProxyController::ProxyController()
{
	this->config.type = COMMAND_PROXY;
	this->config.proxy_client_type = "Http";
	this->config.proxy_server_type = "Http";

	struct file_info info;

	info = { "proxy/proxy.conf", "proxy.conf", proxy_config_transform };
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
	printf("Usage:\n"
		   "    %s proxy <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Flags:\n"
		   "    -c :    client type for proxy [ Http | Redis | SRPC | SRPCHttp"
		   " | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)\n"
		   "    -s :    server type for proxy [ Http | Redis | SRPC | SRPCHttp"
		   " | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)\n"
		   "    -o :    project output path (default: CURRENT_PATH)\n"
		   "    -d :    path of dependencies (default: COMPILE_PATH)\n"
		   , name);
}

void ProxyController::print_success_info() const
{
	printf("Success:\n      make project path \" %s \" done.\n\n",
			this->config.output_path);
	printf("Commands:\n      cd %s\n      make -j\n\n",
			this->config.output_path);
	printf("Execute:\n      ./server\n      ./proxy\n      ./client\n\n");
}

bool ProxyController::copy_files()
{
	struct file_info info;

	if (is_workflow_protocol(this->config.proxy_client_type) &&
		is_workflow_protocol(this->config.proxy_server_type))
	{
		info = { "common/CMakeLists.txt", "CMakeLists.txt", common_cmake_transform };
		this->default_files.push_back(info);

		info = { "common/util.h", "util.h", nullptr };
		this->default_files.push_back(info);

		info = { "proxy/proxy_main.cc", "proxy_main.cc", proxy_proxy_transform };
		this->default_files.push_back(info);

		info = { "proxy/client_main.cc", "client_main.cc", proxy_client_transform };
		this->default_files.push_back(info);

		info = { "proxy/server_main.cc", "server_main.cc", proxy_server_transform };
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
			return false;// TODO: NOT supported yet
	}

	return CommandController::copy_files();
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
			config->proxy_client_type = optarg;
			break;
		case 's':
			config->proxy_server_type = optarg;
			break;
		case 'd':
			config->specified_depend_path = true;
			memset(config->depend_path, 0, MAXPATHLEN);
			if (sscanf(optarg, "%s", config->depend_path) != 1)
				return false;
		default:
			printf("Error:\n     Unknown args : %s\n\n", argv[optind - 1]);
			return false;
		}
	}

	return true;
}

static int check_proxy_type(const char *type)
{
	if (strncasecmp(type, "Http",  strlen("Http") ) == 0 ||
		strncasecmp(type, "Redis", strlen("Redis")) == 0)
		return BASIC_TYPE;

	if (strncasecmp(type, "SRPC",     strlen("SRPC")    ) == 0 ||
		strncasecmp(type, "SRPCHttp", strlen("SRPCHttp")) == 0 ||
		strncasecmp(type, "BRPC",     strlen("BRPC")    ) == 0 ||
		strncasecmp(type, "TRPC",     strlen("TRPC")    ) == 0 ||
		strncasecmp(type, "TRPCHttp", strlen("TRPCHttp")) == 0)
		return PROTOCOL_TYPE;

	if (strncasecmp(type, "Thrift",     strlen("Thrift")    ) == 0 ||
		strncasecmp(type, "ThriftHttp", strlen("ThriftHttp")) == 0)
		return THRIFT_TYPE;

	return -1;
}

bool ProxyController::check_args()
{
	int server_type = check_proxy_type(this->config.proxy_server_type);
	int client_type = check_proxy_type(this->config.proxy_client_type);

	if (CommandController::check_args() == false)
		return false;

	if (client_type < 0 || server_type < 0)
	{
		printf("Error:\n     Invalid type : %s, %s\n\n",
			   this->config.proxy_server_type, this->config.proxy_client_type);
		return false;
	}

	// TODO: temperarily only support workflow to workflow, rpc to rpc
	if ((client_type == 1 && server_type > 1) ||
		(server_type == 1 && client_type > 1))
	{
		printf("Error:\n     Temperarily not support %s and %s together\n\n",
			   this->config.proxy_server_type, this->config.proxy_client_type);
		return false;
	}

	// TODO: temperarily NOT support protobuf with thrift
	if ((client_type == 2 && server_type == 3) ||
		(server_type == 2 && client_type == 3))
	{
		printf("Error:\n     Temperarily not support %s and %s together\n\n",
			   this->config.proxy_server_type, this->config.proxy_client_type);
		return false;
	}

	if (client_type == 2)
	{
		this->config.idl_type = IDL_TYPE_PROTOBUF;
	}
	else if (client_type == 3)
	{
		// this->config.idl_type = IDL_TYPE_THRIFT;
		printf("Error:\n     Temperarily not support IDL thrift.\n\n");
		return false;
	}

	return true;
}

