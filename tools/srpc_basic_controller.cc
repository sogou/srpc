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

#include "srpc_controller.h"

static std::string server_process_codes(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
		return std::string(R"(
    fprintf(stderr, "http server get request_uri: %s\n",
            task->get_req()->get_request_uri());
    print_peer_address<WFHttpTask>(task);

    task->get_resp()->append_output_body("<html>Hello from server!</html>");
)");

	if (type == PROTOCOL_TYPE_REDIS)
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

static std::string client_redirect_codes(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
		return std::string(R"(
                                                        config.redirect_max(),)");

	return std::string("");
}

static std::string client_task_callback_codes(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
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

	if (type == PROTOCOL_TYPE_REDIS)
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

static std::string client_set_request_codes(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
		return std::string(R"(
    protocol::HttpRequest *req = task->get_req();
    req->set_request_uri("/client_request"); // will send to server by proxy
)");

	if (type == PROTOCOL_TYPE_REDIS)
		return std::string(R"(
    task->get_req()->set_request("SET", {"k1", "v1"});
)");

	return std::string("Unknown type");
}

static uint8_t get_protocol_type(const struct srpc_config *config, uint8_t type)
{
	if (config->type == COMMAND_HTTP ||
		(config->type == COMMAND_PROXY && type == PROTOCOL_TYPE_HTTP))
		return PROTOCOL_TYPE_HTTP;

	if (config->type == COMMAND_REDIS ||
		(config->type == COMMAND_PROXY && type == PROTOCOL_TYPE_REDIS))
		return PROTOCOL_TYPE_REDIS;

	if (config->type == COMMAND_MYSQL ||
		(config->type == COMMAND_PROXY && type == PROTOCOL_TYPE_MYSQL))
		return PROTOCOL_TYPE_MYSQL;

	return PROTOCOL_TYPE_MAX;
}

static inline uint8_t get_client_protocol_type(const struct srpc_config *config)
{
	return get_protocol_type(config, config->proxy_client_type);
}

static inline uint8_t get_server_protocol_type(const struct srpc_config *config)
{
	return get_protocol_type(config, config->proxy_server_type);
}

bool basic_server_config_transform(const std::string& format, FILE *out,
								   const struct srpc_config *config)
{
	unsigned short port;

	if (get_server_protocol_type(config) == PROTOCOL_TYPE_HTTP)
		port = 80;
	else if (get_server_protocol_type(config) == PROTOCOL_TYPE_REDIS)
		port = 6379;
	else if (get_server_protocol_type(config) == PROTOCOL_TYPE_MYSQL)
		port = 3306;
	else
		port = 1412;

	size_t len = fprintf(out, format.c_str(), port);

	return len > 0;
}

bool basic_client_config_transform(const std::string& format, FILE *out,
								   const struct srpc_config *config)
{
	unsigned short port;
	std::string redirect_code;

	if (get_client_protocol_type(config) == PROTOCOL_TYPE_HTTP)
	{
		port = 80;
		redirect_code = R"(    "redirect_max": 2,
)";
	}
	else if (get_client_protocol_type(config) == PROTOCOL_TYPE_REDIS)
		port = 6379;
	else if (get_client_protocol_type(config) == PROTOCOL_TYPE_MYSQL)
		port = 3306;
	else
		port = 1412;
	
	// for proxy
	if (config->type == COMMAND_PROXY)
		port = port - 1;

	size_t len = fprintf(out, format.c_str(), port, redirect_code.c_str());

	return len > 0;
}

bool basic_server_transform(const std::string& format, FILE *out,
							const struct srpc_config *config)
{
	uint8_t server_type = get_server_protocol_type(config);
	const char *type = get_type_string(server_type);
	size_t len = fprintf(out, format.c_str(), type, type,
						 server_process_codes(server_type).c_str(),
						 type, type);

	return len > 0;
}

bool basic_client_transform(const std::string& format, FILE *out,
							const struct srpc_config *config)
{
	uint8_t client_type = get_client_protocol_type(config);
	const char *type = get_type_string(client_type);
	std::string client_lower = type;
	std::transform(client_lower.begin(), client_lower.end(),
				   client_lower.begin(), ::tolower);

	size_t len = fprintf(out, format.c_str(), type, type, type,
						 client_task_callback_codes(client_type).c_str(),
						 client_lower.c_str(), type, client_lower.c_str(),
						 client_redirect_codes(client_type).c_str(),
						 client_set_request_codes(client_type).c_str());

	return len > 0;
}

HttpController::HttpController()
{
	this->config.type = COMMAND_HTTP;
	struct file_info info;

	info = { "basic/server.conf", "server.conf", basic_server_config_transform };
	this->default_files.push_back(info);

	info = { "basic/client.conf", "client.conf", basic_client_config_transform };
	this->default_files.push_back(info);

	info = { "basic/server_main.cc", "server_main.cc", basic_server_transform };
	this->default_files.push_back(info);

	info = { "basic/client_main.cc", "client_main.cc", basic_client_transform };
	this->default_files.push_back(info);

	info = { "common/config.json", "example.conf", nullptr };
	this->default_files.push_back(info);

	info = { "common/util.h", "config/util.h", nullptr };
	this->default_files.push_back(info);

	info = { "common/CMakeLists.txt", "CMakeLists.txt", common_cmake_transform };
	this->default_files.push_back(info);

	info = {"common/GNUmakefile", "GNUmakefile", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.h", "config/Json.h", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.cc", "config/Json.cc", nullptr };
	this->default_files.push_back(info);

	info = {"config/config_simple.h", "config/config.h", nullptr};
	this->default_files.push_back(info);

	info = {"config/config_simple.cc", "config/config.cc", nullptr};
	this->default_files.push_back(info);
}

void HttpController::print_usage(const char *name) const
{
	printf("Usage:\n"
		   "    %s http <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Flags:\n"
		   "    -o :    project output path (default: CURRENT_PATH)\n"
		   "    -d :    path of dependencies (default: COMPILE_PATH)\n"
		   , name);
}

bool HttpController::get_opt(int argc, const char **argv)
{
	struct srpc_config *config = &this->config;
	char c;
	optind = 3;

	while ((c = getopt(argc, (char * const *)argv, "o:t:d:")) != -1)
	{
		switch (c)
		{
		case 'o':
			if (sscanf(optarg, "%s", config->output_path) != 1)
				return false;
			break;
		case 't':
			if (sscanf(optarg, "%s", config->template_path) != 1)
				return false; //TODO:
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

