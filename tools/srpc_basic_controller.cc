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

using DEFAULT_FILES = std::vector<struct CommandController::file_info>;

static std::string server_process_codes(uint8_t type)
{
	if (type == PROTOCOL_TYPE_HTTP)
		return std::string(R"(
    fprintf(stderr, "http server get request_uri: %s\n",
            task->get_req()->get_request_uri());
    print_peer_address(task);

    task->get_resp()->append_output_body("<html>Hello from server!</html>");)");

	if (type == PROTOCOL_TYPE_REDIS)
		return std::string(R"(
        protocol::RedisRequest *req   = task->get_req();
        protocol::RedisResponse *resp = task->get_resp();
        protocol::RedisValue val;
        std::string cmd;

        if (req->parse_success() == false || req->get_command(cmd) == false)
            return;

        fprintf(stderr, "redis server get cmd: [%s] from ", cmd.c_str());
        print_peer_address(task);

        val.set_status("OK"); // example: return OK to every requests
        resp->set_result(val);)");

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
     })");

	if (type == PROTOCOL_TYPE_REDIS)
		return std::string(R"(
    protocol::RedisResponse *resp = task->get_resp();
    protocol::RedisValue val;

    if (state == WFT_STATE_SUCCESS && resp->parse_success() == true)
    {
        resp->get_result(val);
        fprintf(stderr, "response: %s\n", val.string_value().c_str());
    })");

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

static std::string username_passwd_codes(uint8_t type)
{
	if (type == PROTOCOL_TYPE_REDIS || type == PROTOCOL_TYPE_MYSQL)
		return std::string(R"(config.client_user_name() +
                      std::string(":") + config.client_password() +
                      std::string("@") +)");

	return std::string("");
}

static uint8_t get_protocol_type(const struct srpc_config *config, uint8_t type)
{
	if (config->type == COMMAND_HTTP || config->type == COMMAND_COMPUTE ||
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
		port = 8080;
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
	std::string user_and_passwd;

	if (get_client_protocol_type(config) == PROTOCOL_TYPE_HTTP)
	{
		port = 8080;
		redirect_code = R"(
    "redirect_max": 2,)";
	}
	else if (get_client_protocol_type(config) == PROTOCOL_TYPE_REDIS)
	{
		port = 6379;
		user_and_passwd = R"(,
    "user_name": "root",
    "password": "")";
	}
	else if (get_client_protocol_type(config) == PROTOCOL_TYPE_MYSQL)
		port = 3306;
	else
		port = 1412;
	
	// for proxy
	if (config->type == COMMAND_PROXY)
	{
		if (get_client_protocol_type(config) == PROTOCOL_TYPE_HTTP)
			port = 8888;
		else
			port = port - 1;
	}

	size_t len = fprintf(out, format.c_str(), port,
						 redirect_code.c_str(), user_and_passwd.c_str());

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

	std::string client_request_codes;
	if ((config->type == COMMAND_PROXY && client_type == PROTOCOL_TYPE_HTTP) ||
		config->type == COMMAND_REDIS)
		client_request_codes = client_set_request_codes(client_type);

	size_t len = fprintf(out, format.c_str(), type, type, type,
						 client_task_callback_codes(client_type).c_str(),
						 client_lower.c_str(),
						 username_passwd_codes(client_type).c_str(),
						 type, client_lower.c_str(),
						 client_redirect_codes(client_type).c_str(),
						 client_request_codes.c_str());

	return len > 0;
}

static void basic_default_file_initialize(DEFAULT_FILES& files)
{
	struct CommandController::file_info info;

	info = { "basic/server.conf", "server.conf", basic_server_config_transform };
	files.push_back(info);

	info = { "basic/client.conf", "client.conf", basic_client_config_transform };
	files.push_back(info);

	info = { "basic/server_main.cc", "server_main.cc", basic_server_transform };
	files.push_back(info);

	info = { "basic/client_main.cc", "client_main.cc", basic_client_transform };
	files.push_back(info);

	info = { "common/config.json", "full.conf", nullptr };
	files.push_back(info);

	info = { "common/util.h", "config/util.h", nullptr };
	files.push_back(info);

	info = { "common/CMakeLists.txt", "CMakeLists.txt", common_cmake_transform };
	files.push_back(info);

	info = { "common/GNUmakefile", "GNUmakefile", nullptr };
	files.push_back(info);

	info = { "config/Json.h", "config/Json.h", nullptr };
	files.push_back(info);

	info = { "config/Json.cc", "config/Json.cc", nullptr };
	files.push_back(info);

	info = { "config/config_simple.h", "config/config.h", nullptr };
	files.push_back(info);

	info = { "config/config_simple.cc", "config/config.cc", nullptr };
	files.push_back(info);
}

static bool get_opt_args(int argc, const char **argv, struct srpc_config *config)
{
	int c;

	while ((c = getopt(argc, (char * const *)argv, "o:t:d:")) >= 0)
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
			break;
		default:
			printf(COLOR_RED"Error:\n     Unknown args : "
				   COLOR_BLUE"%s\n\n" COLOR_OFF, argv[optind - 1]);
			return false;
		}
	}

	return true;
}

static bool basic_get_opt(int argc, const char **argv, struct srpc_config *config)
{
	optind = 2;

	if (get_opt_args(argc, argv, config) == false)
		return false;

	if (optind == argc)
	{
		printf(COLOR_RED "Missing: PROJECT_NAME\n\n" COLOR_OFF);
		return false;
	}

	config->project_name = argv[optind];
	optind++;

	if (get_opt_args(argc, argv, config) == false)
		return false;

	if (config->project_name == NULL)
	{
		printf(COLOR_RED "Missing: PROJECT_NAME\n\n" COLOR_OFF);
		return false;
	}

	return true;
}

static void basic_print_usage(const char *name, const char *command)
{
	printf(COLOR_PINK"Usage:\n"
		   COLOR_INFO"    %s " COLOR_COMMAND "%s "
		   COLOR_INFO"<PROJECT_NAME>" COLOR_FLAG " [FLAGS]\n\n"
		   COLOR_PINK"Example:\n"
		   COLOR_PURPLE"    %s %s my_%s_project\n\n"
		   COLOR_PINK"Available Flags:\n"
		   COLOR_FLAG"    -o : "
		   COLOR_WHITE"project output path (default: CURRENT_PATH)\n"
		   COLOR_FLAG"    -d : "
		   COLOR_WHITE"path of dependencies (default: COMPILE_PATH)\n"
		   COLOR_OFF, name, command, name, command, command);
}

HttpController::HttpController()
{
	this->config.type = COMMAND_HTTP;
	basic_default_file_initialize(this->default_files);
}

void HttpController::print_usage(const char *name) const
{
	basic_print_usage(name, "http");
}

bool HttpController::get_opt(int argc, const char **argv)
{
	return basic_get_opt(argc, argv, &this->config);
}

RedisController::RedisController()
{
	this->config.type = COMMAND_REDIS;
	basic_default_file_initialize(this->default_files);
}

void RedisController::print_usage(const char *name) const
{
	basic_print_usage(name, "redis");
}

bool RedisController::get_opt(int argc, const char **argv)
{
	return basic_get_opt(argc, argv, &this->config);
}

FileServiceController::FileServiceController()
{
	this->config.type = COMMAND_FILE;

	struct file_info info;

	info = { "file/server.conf", "server.conf", nullptr };
	this->default_files.push_back(info);

	info = { "file/server_main.cc", "server_main.cc", nullptr };
	this->default_files.push_back(info);

	info = { "file/file_service.cc", "file_service.cc", nullptr };
	this->default_files.push_back(info);

	info = { "file/file_service.h", "file_service.h", nullptr };
	this->default_files.push_back(info);

	info = { "file/index.html", "html/index.html", nullptr };
	this->default_files.push_back(info);

	info = { "file/404.html", "html/404.html", nullptr };
	this->default_files.push_back(info);

	info = { "file/50x.html", "html/50x.html", nullptr };
	this->default_files.push_back(info);

	info = { "common/CMakeLists.txt", "CMakeLists.txt", common_cmake_transform };
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

void FileServiceController::print_usage(const char *name) const
{
	basic_print_usage(name, "file");
}

bool FileServiceController::get_opt(int argc, const char **argv)
{
	return basic_get_opt(argc, argv, &this->config);
}

void FileServiceController::print_success_info() const
{
	printf(COLOR_GREEN"Success:\n      make project path "
		   COLOR_BLUE"\" %s \"" COLOR_GREEN "done.\n\n",
		   this->config.output_path);
	printf(COLOR_PINK"Commands:\n      " COLOR_BLUE "cd %s\n      make -j\n\n",
		   this->config.output_path);
	printf(COLOR_PINK"Execute:\n      " COLOR_GREEN "./server\n\n");
	printf(COLOR_PINK"Try file service:\n");
	printf(COLOR_WHITE"      curl localhost:8080/index.html\n");
	printf("      curl -i localhost:8080/a/b/\n\n" COLOR_OFF);
}

ComputeController::ComputeController()
{
	this->config.type = COMMAND_COMPUTE;
	struct CommandController::file_info info;

	info = { "basic/server.conf", "server.conf", basic_server_config_transform };
	this->default_files.push_back(info);

	info = { "basic/compute_server_main.cc", "server_main.cc", nullptr };
	this->default_files.push_back(info);

	info = { "common/CMakeLists.txt", "CMakeLists.txt", common_cmake_transform };
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

void ComputeController::print_usage(const char *name) const
{
	basic_print_usage(name, "compute");
}

bool ComputeController::get_opt(int argc, const char **argv)
{
	return basic_get_opt(argc, argv, &this->config);
}

void ComputeController::print_success_info() const
{
	printf(COLOR_GREEN"Success:\n      make project path "
		   COLOR_BLUE"\" %s \"" COLOR_GREEN " done.\n\n",
		   this->config.output_path);
	printf(COLOR_PINK"Commands:\n      " COLOR_BLUE "cd %s\n      make -j\n\n",
		   this->config.output_path);
	printf(COLOR_PINK"Execute:\n      " COLOR_GREEN "./server\n\n");
	printf(COLOR_PINK"Try compute with n=8:\n");
	printf(COLOR_WHITE"      curl localhost:8080/8\n\n" COLOR_OFF);
}

