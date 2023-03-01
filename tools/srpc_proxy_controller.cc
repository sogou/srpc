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

static std::unordered_map<std::string, unsigned short> default_server_port = {
	{"Http",  8080},
	{"Redis", 6379},
	{"Mysql", 3306},
	{"SRPC",  1412},
};

static std::unordered_map<std::string, unsigned short> default_proxy_port = {
	{"Http",  8888},
	{"Redis", 6378},
	{"Mysql", 3305},
	{"SRPC",  1411},
};

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
        printf("http server get request_uri: %s\n",
               task->get_req()->get_request_uri());

        task->get_resp()->append_output_body("<html>Hello from server!</html>");
)");

	if (type.compare("Redis") == 0)
		return std::string(R"(
        protocol::RedisRequest *req   = task->get_req();
        protocol::RedisResponse *resp = task->get_resp();
        protocol::RedisValue val;
        std::string cmd;

        if (req->parse_success() == false || req->get_command(cmd) == false)
            return false;

        fprintf(stderr, "redis server get cmd: [%s] from ", cmd.c_str());
        print_peer_address(task);

        val.set_status("OK"); // example: return OK to every requests
        resp->set_result(val);
)");

	return std::string("Unknown type");
}

static std::string proxy_callback_response_codes(const std::string& type)
{
	if (type.compare("Http") == 0)
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
                                                              config.redirect_max(),
)");

	return std::string("\n");
}

static bool is_workflow_protocol(const char *type)
{
	if (strncasecmp(type, "Http",  strlen("Http")) == 0 ||
		strncasecmp(type, "Redis", strlen("Redis")) == 0)
		return true;

	return false;
}

static bool proxy_cmake_transform(const std::string& format, FILE *out,
								  const struct srpc_config *config)
{
	std::string path = config->depend_path;
	path += "workflow";

	size_t len = fprintf(out, format.c_str(), config->project_name, path.c_str());

	return len > 0;
}

static bool proxy_util_transform(const std::string& format, FILE *out,
								  const struct srpc_config *config)
{
	// TODO: RPC types
	std::string task_type = "WF";
	task_type += config->proxy_server_type_string();
	task_type += "Task";

	size_t len = fprintf(out, format.c_str(), task_type.c_str());

	return len > 0;
}

static bool proxy_proxy_transform(const std::string& format, FILE *out,
								  const struct srpc_config *config)
{
	std::string server_type = config->proxy_server_type_string();
	std::string client_type = config->proxy_client_type_string();
	std::string client_lower = client_type;
	std::transform(client_lower.begin(), client_lower.end(),
				   client_lower.begin(), ::tolower);
	std::string server_port = std::to_string(default_server_port[server_type]);
	std::string proxy_port = std::to_string(default_proxy_port[client_type]);

	size_t len = fprintf(out, format.c_str(), server_type.c_str(),
						 server_type.c_str(), server_type.c_str(),
						 client_type.c_str(), client_type.c_str(),
						 proxy_callback_response_codes(server_type).c_str(),
						 // process
						 client_type.c_str(), client_type.c_str(),
						 client_lower.c_str(),
						 client_type.c_str(), client_lower.c_str(),
						 proxy_redirect_codes(client_type).c_str(),
						 client_type.c_str(),
						 // main
						 client_type.c_str(), client_type.c_str());

	return len > 0;
}

static bool proxy_client_transform(const std::string& format, FILE *out,
								   const struct srpc_config *config)
{
	std::string client_type = config->proxy_client_type_string();
	std::string client_lower = client_type;
	std::transform(client_lower.begin(), client_lower.end(),
				   client_lower.begin(), ::tolower);
	std::string proxy_port = std::to_string(default_proxy_port[client_type]);

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
	std::string server_port = std::to_string(default_server_port[server_type]);

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
	std::string server_port = config->proxy_server_type_string();
	server_port = std::to_string(default_server_port[server_port]);

	std::string proxy_port = config->proxy_client_type_string();
	proxy_port = std::to_string(default_proxy_port[proxy_port]);

	size_t len = fprintf(out, format.c_str(),
						 proxy_port.c_str(), server_port.c_str());

	return len > 0;
}

ProxyController::ProxyController()
{
	this->config.type = COMMAND_PROXY;
	struct file_info info;

	info = { "common/util.h", "util.h", proxy_util_transform };
	this->default_files.push_back(info);

	info = { "proxy/CMakeLists.txt", "CMakeLists.txt", proxy_cmake_transform };
	this->default_files.push_back(info);

	info = { "proxy/config.json", "config.json", proxy_config_transform };
	this->default_files.push_back(info);

	info = { "common/GNUmakefile", "GNUmakefile", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.h", "config/Json.h", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.cc", "config/Json.cc", nullptr };
	this->default_files.push_back(info);

	info = { "config/config_simple.h", "config/config.h", nullptr };
	this->default_files.push_back(info);

	info = { "config/config_simple.cc", "config/config.cc", nullptr };
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

bool ProxyController::copy_files()
{
	struct file_info info;

	if (is_workflow_protocol(config.proxy_client_type) &&
		is_workflow_protocol(config.proxy_server_type))
	{
		info = { "proxy/proxy_main.cc", "proxy_main.cc", proxy_proxy_transform };
		this->default_files.push_back(info);

		info = { "proxy/client_main.cc", "client_main.cc", proxy_client_transform };
		this->default_files.push_back(info);

		info = { "proxy/server_main.cc", "server_main.cc", proxy_server_transform };
		this->default_files.push_back(info);
	}
	else
	{
		//TODO: is rpc
	}

	return CommandController::copy_files();
}

bool ProxyController::get_opt(int argc, const char **argv)
{
	struct srpc_config *config = &this->config;
	char c;
	optind = 3;

	while ((c = getopt(argc, (char * const *)argv,
					   "o:c:s:d:")) != -1)
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

static bool check_proxy_type(const char *type)
{
	if (strncasecmp(type, "Http",       strlen("Http")      ) != 0 &&
		strncasecmp(type, "Redis",      strlen("Redis")     ) != 0 &&
		strncasecmp(type, "SRPC",       strlen("SRPC")      ) != 0 &&
		strncasecmp(type, "SRPCHttp",   strlen("SRPCHttp")  ) != 0 &&
		strncasecmp(type, "BRPC",       strlen("BRPC")      ) != 0 &&
		strncasecmp(type, "Thrift",     strlen("Thrift")    ) != 0 &&
		strncasecmp(type, "ThriftHttp", strlen("ThriftHttp")) != 0 &&
		strncasecmp(type, "TRPC",       strlen("TRPC")      ) != 0 &&
		strncasecmp(type, "TRPCHttp",   strlen("TRPCHttp")  ) != 0)
	return false;

	return true;
}

bool ProxyController::check_args()
{
	if (CommandController::check_args() == false)
		return false;

	if (check_proxy_type(config.proxy_client_type) == false)
	{
		printf("Error:\n     Invalid client type :%s\n\n",
			   config.proxy_client_type);
		return false;
	}

	if (check_proxy_type(config.proxy_server_type) == false)
	{
		printf("Error:\n     Invalid server type :%s\n\n",
			   config.proxy_server_type);
		return false;
	}

	// TODO: temperarily only support workflow to workflow, rpc to rpc
	const char *server_type = config.proxy_server_type;
	const char *client_type = config.proxy_client_type;

	if ((is_workflow_protocol(server_type) == true &&
		is_workflow_protocol(client_type) == false) ||
		(is_workflow_protocol(client_type) == true &&
		is_workflow_protocol(server_type) == false))
	{
		printf("Error:\n     Temperarily not support %s and %s together\n\n",
			   server_type, client_type);
		return false;
	}

	return true;
}

