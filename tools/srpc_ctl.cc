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

#include "srpc_config.h"

static constexpr const char *CMAKE_PROTOC_CODES = R"(
find_program(PROTOC "protoc")
if(${PROTOC} STREQUAL "PROTOC-NOTFOUND")
    message(FATAL_ERROR "Protobuf compiler is missing!")
endif ()
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${IDL_FILE}))";

static bool open_project_dir(struct srpc_config *config)
{
	if (opendir(config->output_path) != NULL)
	{
		printf("Error:\n      project path \" %s \" EXISTS.\n\n",
				config->output_path);
		return false;
	}

	if (mkdir_p(config->output_path, 0755) != 0)
	{
		perror("Error:\n      failed to make project ");
		return false;
	}

	std::string config_path = config->output_path;
	config_path += "/config";
	if (mkdir(config_path.c_str(), 0755) != 0)
	{
		perror("Error:\n      failed to make project config path ");
		return false;
	}

	printf("Success:\n      make project path \" %s \" done.\n\n",
			config->output_path);
	printf("Command:\n      cd %s\n      make -j\n\n", config->output_path);
	printf("Execute:\n      ./server\n      ./client\n\n");
	return true;
}

// get the depth-th upper path of file
static bool get_path(const char *file, char *path, int depth)
{
	size_t len = strlen(file);
	size_t i;

	memset(path, 0, MAXPATHLEN);

	if (len == 0 || depth <= 0)
		return false;
	
	int state = 0;

	for (i = len - 1; i >= 0; i--)
	{
		if (file[i] == '/')
		{
			state++;
			if (state >= depth)
				break;
		}
	}

	if (state != depth)
		return false;

	memcpy(path, file, i + 1);
	return true;
}

bool parse_args(int argc, const char **argv, struct srpc_config *config)
{
	if (argc < 3)
	{
		printf("Missing: PROJECT_NAME\n\n");
		return false;
	}

	getcwd(config->output_path, MAXPATHLEN);
	config->project_name = argv[2];
	config->service_name = argv[2];
	if (get_path(__FILE__, config->depend_path, 2) == false)
		return false;

	char c;
	optind = 3;

	while ((c = getopt(argc, (char * const *)argv,
					   "o:r:i:x:c:s:t:d:f:p:")) != -1)
	{
		switch (c)
		{
		case 'o':
			if (sscanf(optarg, "%s", config->output_path) != 1)
				return false;
			break;
		case 'r':
			config->set_rpc_type(optarg);
			break;
		case 'i':
			config->set_idl_type(optarg);
			break;
		case 'x':
			config->set_data_type(optarg);
			break;
		case 'c':
			config->set_compress_type(optarg);
			break;
		case 's':
			config->service_name = optarg;
			break;
		case 't':
			config->template_path = optarg; // TODO: 
			break;
		case 'd':
			config->specified_depend_path = true;
			memset(config->depend_path, 0, MAXPATHLEN);
			if (sscanf(optarg, "%s", config->depend_path) != 1)
				return false;
		case 'f':
			config->specified_idl_file = optarg;
			break;
		case 'p':
			config->specified_idl_path = optarg;
			break;
		default:
			printf("Error:\n     Unknown args : %s\n\n", argv[optind - 1]);
			return false;
		}
	}

	if (config->prepare_args() == false || open_project_dir(config) == false)
		return false;

	return true;
}

static bool generate_from_template_http(const char *file_name,
										const struct srpc_config *config,
										char *format,
										size_t size)
{
	FILE *write_fp;
	std::string write_file = config->output_path;

	size_t name_len = strlen(file_name);

	if (strncmp(file_name, "config_simple.h", name_len) == 0)
		write_file += "config/config.h";
	else if (strncmp(file_name, "config_simple.cc", name_len) == 0)
		write_file += "config/config.cc";
	else if (strncmp(file_name, "Json.h", name_len) == 0 ||
			 strncmp(file_name, "Json.cc", name_len) == 0)
	{
		write_file += "config/";
		write_file += file_name;
	}
	else if (strncmp(file_name, "config_full.h", name_len) == 0 ||
			 strncmp(file_name, "config_full.cc", name_len) == 0)
		return true;
	else
		write_file += file_name;

	write_fp = fopen(write_file.c_str(), "w");
	if (!write_fp)
	{
		printf("Error:\n      open \" %s \" failed\n", write_file.c_str());
		return false;
	}

	size_t len;
	std::string path = config->depend_path;
	path += "workflow";

	if (strncmp(file_name, "CMakeLists.txt", name_len) == 0)
	{
		std::string f = std::string(format, size);
		len = fprintf(write_fp, f.c_str(), config->project_name, path.c_str());
	}
	else
		len = fwrite(format, size, 1, write_fp);

	fclose(write_fp);
	return len == 1;
}

static std::string rpc_client_file_codes(const struct srpc_config *config)
{
	std::string ret;

	if (config->compress_type != COMPRESS_TYPE_NONE)
	{
		ret += "\tparams.task_params.compress_type = ";
		ret += config->rpc_compress_string();
		ret += ";\n";
	}

	if (config->data_type != DATA_TYPE_DEFAULT)
	{
		ret += "\tparams.task_params.data_type = ";
		ret += config->rpc_data_string();
		ret += ";\n";
	}

	return ret;
}

static std::string rpc_server_file_codes(const struct srpc_config *config)
{
	std::string ret;

	if (config->compress_type != COMPRESS_TYPE_NONE)
	{
		ret += "\t\tctx->set_compress_type(";
		ret += config->rpc_compress_string();
		ret += ");\n";
	}

	if (config->data_type != DATA_TYPE_DEFAULT)
	{
		ret += "\t\tctx->set_data_type(";
		ret += config->rpc_data_string();
		ret += ");\n";
	}

	ret += "\t\t";

	return ret;
}

static bool generate_from_template_rpc(const char *file_name,
									   const struct srpc_config *config,
									   char *format,
									   size_t size)
{
	size_t len;
	std::string srpc_path = config->depend_path;
	std::string workflow_path = config->depend_path;
	workflow_path += "workflow";

	FILE *write_fp;
	std::string write_file = config->output_path;

	size_t name_len = strlen(file_name);

	const char *idl_file_name;
	const char *client_file_name;
	const char *server_file_name;

	if (config->idl_type == IDL_TYPE_PROTOBUF)
	{
		idl_file_name = "rpc.proto";
		client_file_name = "client_protobuf.cc";
		server_file_name = "server_protobuf.cc";
	}
	else
	{
		idl_file_name = "rpc.thrift";
		client_file_name = "client_thrift.cc";
		server_file_name = "server_thrift.cc";
	}

	if (config->is_rpc_skip_file(file_name) == true)
		return true;
	else if (strncmp(file_name, idl_file_name, name_len) == 0)
	{
		write_file += config->service_name;
		write_file += std::string(idl_file_name + 3);
	}
	else if (strncmp(file_name, "Json.h", name_len) == 0 ||
			 strncmp(file_name, "Json.cc", name_len) == 0)
	{
		write_file += "config/";
		write_file += file_name;
	}
	else if (strncmp(file_name, "config_full.h", name_len) == 0)
		write_file += "config/config.h";
	else if (strncmp(file_name, "config_full.cc", name_len) == 0)
		write_file += "config/config.cc";
	else if (strncmp(file_name, server_file_name, name_len) == 0)
		write_file += "server_main.cc";
	else if (strncmp(file_name, client_file_name, name_len) == 0)
		write_file += "client_main.cc";
	else
		write_file += file_name;

	write_fp = fopen(write_file.c_str(), "w");
	if (!write_fp)
	{
		printf("Error:\n      open \" %s \" failed\n", write_file.c_str());
		return false;
	}

	std::string f = std::string(format, size);

	if (strncmp(file_name, "CMakeLists.txt", name_len) == 0)
	{
		std::string idl_file_name;

		if (config->specified_idl_file != NULL)
		{
			idl_file_name = config->specified_idl_file;
			size_t pos = idl_file_name.find_last_of('/');
			if (pos != std::string::npos)
				idl_file_name = idl_file_name.substr(pos + 1);
		}
		else if (config->idl_type == IDL_TYPE_PROTOBUF)
		{
			idl_file_name = config->project_name;
			idl_file_name += ".proto";
		}
		else
		{
			idl_file_name = config->project_name;
			idl_file_name += ".thrift";
		}

		len = fprintf(write_fp, f.c_str(), config->project_name,
					  workflow_path.c_str(), srpc_path.c_str(),
					  idl_file_name.c_str(),
					  config->idl_type == IDL_TYPE_PROTOBUF ?
					  CMAKE_PROTOC_CODES : "");
	}
	else if (strncmp(file_name, idl_file_name, name_len) == 0)
	{
		len = fprintf(write_fp, f.c_str(), config->service_name);
	}
	else if (strncmp(file_name, client_file_name, name_len) == 0)
	{
		std::string prepare_params = rpc_client_file_codes(config);
		len = fprintf(write_fp, f.c_str(),
					  config->service_name, prepare_params.c_str(),
					  config->service_name, config->rpc_type_string());
	}
	else if (strncmp(file_name, server_file_name, name_len) == 0)
	{
		std::string prepare_ctx = rpc_server_file_codes(config);
		len = fprintf(write_fp, f.c_str(),
					  config->service_name, config->service_name,
					  config->service_name, prepare_ctx.c_str(),
					  config->rpc_type_string(), config->service_name,
					  config->project_name, config->rpc_type_string());
	}
	else
		len = fwrite(format, size, 1, write_fp);

	fclose(write_fp);
	return len == 1;
}

static bool generate_from_template(const char *file_name,
								   const struct srpc_config *config,
								   char *format,
								   size_t size)
{
	switch (config->type)
	{
	case COMMAND_HTTP:
		return generate_from_template_http(file_name, config, format, size);
	case COMMAND_RPC:
		return generate_from_template_rpc(file_name, config, format, size);
	default:
		return false;
	}
}

bool copy_path_files(const char *path, const struct srpc_config *config)
{
	DIR *dir = NULL;
	dir = opendir(path);

	if (dir == NULL)
	{
		printf("Error:\n      \" %s \" does NOT exist.\n\n", path);
		return false;
	}

	struct dirent *entry;
	FILE *read_fp;
	size_t size;
	ssize_t len;
	char *buf;

	int left_file_count = 0;

	std::string read_file;

	while ((entry = readdir(dir)) != NULL)
	{
		if (entry->d_name[0] == '.')
			continue;

		left_file_count++;
		read_file = path;
		read_file += entry->d_name;

		read_fp = fopen(read_file.c_str(), "r");
		if (read_fp)
		{
			fseek(read_fp, 0, SEEK_END);
			size = ftell(read_fp);
			buf = (char *)malloc(size);
			if (buf)
			{
				fseek(read_fp, 0, SEEK_SET);
				len = fread(buf, size, 1, read_fp);

				if (len == 1)
				{
					if (generate_from_template(entry->d_name, config, buf, size))
						left_file_count--;
				}
				else
					printf("Error:\n       read \" %s \" failed\n\n",
							read_file.c_str());

				free(buf);
			}
			else
				printf("Error:\n      system error.\n\n");

			fclose(read_fp);
		}
		else
		{
			printf("Error:\n      open \" %s \" failed.\n\n",
				   read_file.c_str());
		}
	}

	closedir(dir);

	return left_file_count == 0;
}

void parse_http(int argc, const char *argv[], struct srpc_config *config)
{
	config->type = COMMAND_HTTP;

	if (parse_args(argc, argv, config) == false)
	{
		usage_http(argc, argv);
		return;
	}

	if (opendir(config->template_path) == NULL)
	{
		printf("Error:\n      template path \" %s \" does NOT exist.\n",
				config->template_path);
		return;
	}

	std::string path = config->template_path;
	path += "/common/";
	copy_path_files(path.c_str(), config);

	path = config->template_path;
	path += "/config/";
	copy_path_files(path.c_str(), config);

	path = config->template_path;
	path += "/http/";
	copy_path_files(path.c_str(), config);
}

void parse_db(int argc, const char *argv[], struct srpc_config *config)
{
	if (strcasecmp(argv[1], "redis") == 0)
	{
		config->type = COMMAND_REDIS;
		config->server_url = "redis://127.0.0.1:6379";
	}
	else if (strcasecmp(argv[1], "mysql") == 0)
	{
		config->type = COMMAND_MYSQL;
		config->server_url = "mysql://127.0.0.1:3306";
	}
	else
		return;

	if (parse_args(argc, argv, config) == false)
	{
		usage_db(argc, argv, config);
		return;
	}

	// copy files
}

void parse_kafka(int argc, const char *argv[], struct srpc_config *config)
{
	config->type = COMMAND_KAFKA;
	config->server_url = "kafka://127.0.0.1:9092";
	if (parse_args(argc, argv, config) == false)
	{
		usage_kafka(argc, argv);
		return;
	}

	// copy files
}

void parse_rpc(int argc, const char *argv[], struct srpc_config *config)
{
	config->type = COMMAND_RPC;

	if (parse_args(argc, argv, config) == false)
	{
		usage_rpc(argc, argv, config);
		return;
	}

	std::string path = config->template_path;
	path += "/common/";
	copy_path_files(path.c_str(), config);

	path = config->template_path;
	path += "/config/";
	copy_path_files(path.c_str(), config);

	path = config->template_path;
	path += "/rpc/";
	copy_path_files(path.c_str(), config);

	if (config->specified_idl_file != NULL)
	{
		char idl_path[MAXPATHLEN] = {};

		if (config->specified_idl_file[0] != '/' &&
			config->specified_idl_file[1] != ':')
		{
			char current_dir[MAXPATHLEN] = {};
			getcwd(current_dir, MAXPATHLEN);
			std::string path = current_dir;
			path += "/";
			path += config->specified_idl_file;
			get_path(path.data(), idl_path, 1);
		}
		else
			get_path(config->specified_idl_file, idl_path, 1);

		copy_path_files(idl_path, config); // TODO: copy relevant proto files

		ControlGenerator gen(config);
		struct GeneratorParams params;
		params.out_dir = config->output_path;
		params.input_dir = config->specified_idl_path;

		params.idl_file = config->specified_idl_file;
		auto pos = params.idl_file.find_last_of('/');
		if (pos != std::string::npos)
			params.idl_file = params.idl_file.substr(pos + 1);

		printf("Info: srpc-ctl generator begin.\n");
		gen.generate(params);
		printf("Info: srpc-ctl generator done.\n");
	}
}

void parse_extra(int argc, const char *argv[], struct srpc_config *config)
{
	//TODO:
}

int main(int argc, const char *argv[])
{
	if (argc < 2)
	{
		usage(argc, argv);
		return 0;
	}

	struct srpc_config config;

	if (strcasecmp(argv[1], "http") == 0)
	{
		parse_http(argc, argv, &config);
	}
	else if (strcasecmp(argv[1], "redis") == 0 ||
			 strcasecmp(argv[1], "mysql") == 0)
	{
		parse_db(argc, argv, &config);
	}
	else if (strcasecmp(argv[1], "kafka") == 0)
	{
		parse_kafka(argc, argv, &config);
	}
	else if (strcasecmp(argv[1], "rpc") == 0)
	{
		parse_rpc(argc, argv, &config);
	}
	else if (strcasecmp(argv[1], "extra") == 0)
	{
		parse_extra(argc, argv, &config);
	}
	else
	{
		usage(argc, argv);
	}

	return 0;
}
