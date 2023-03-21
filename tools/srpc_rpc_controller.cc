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

RPCController::RPCController()
{
	this->config.type = COMMAND_RPC;
	struct file_info info;

	info = { "rpc/CMakeLists.txt", "CMakeLists.txt", rpc_cmake_transform };
	this->default_files.push_back(info);

	info = { "common/config.json", "client.conf", nullptr };
	this->default_files.push_back(info);

	info = { "common/config.json", "server.conf", nullptr };
	this->default_files.push_back(info);

	info = { "common/GNUmakefile", "GNUmakefile", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.h", "config/Json.h", nullptr };
	this->default_files.push_back(info);

	info = { "config/Json.cc", "config/Json.cc", nullptr };
	this->default_files.push_back(info);

	info = { "config/config_full.h", "config/config.h", nullptr };
	this->default_files.push_back(info);

	info = { "config/config_full.cc", "config/config.cc", nullptr };
	this->default_files.push_back(info);
}

void RPCController::print_usage(const char *name) const
{
	printf(COLOR_PINK"Usage:\n"
		   COLOR_INFO"    %s " COLOR_COMMAND "rpc "
		   COLOR_INFO"<PROJECT_NAME> " COLOR_FLAG "[FLAGS]\n\n"
		   COLOR_PINK"Example:\n"
		   COLOR_PURPLE"    %s rpc my_rpc_project\n\n"
		   COLOR_PINK"Available Flags:\n"
		   COLOR_FLAG"    -r "
		   COLOR_WHITE": rpc type [ SRPC | SRPCHttp | BRPC | Thrift | "
		   "ThriftHttp | TRPC | TRPCHttp ] (default: SRPC)\n"
		   COLOR_FLAG"    -o "
		   COLOR_WHITE": project output path (default: CURRENT_PATH)\n"
		   COLOR_FLAG"    -s "
		   COLOR_WHITE": service name (default: PROJECT_NAME)\n"
		   COLOR_FLAG"    -i "
		   COLOR_WHITE": idl type [ protobuf | thrift ] (default: protobuf)\n"
		   COLOR_FLAG"    -x "
		   COLOR_WHITE": data type [ protobuf | thrift | json ] "
		   "(default: idl type. json for http)\n"
		   COLOR_FLAG"    -c "
		   COLOR_WHITE": compress type [ gzip | zlib | snappy | lz4 ] "
		   "(default: no compression)\n"
		   COLOR_FLAG"    -d "
		   COLOR_WHITE": path of dependencies (default: COMPILE_PATH)\n"
		   COLOR_FLAG"    -f "
		   COLOR_WHITE": specify the idl_file to generate codes "
		   "(default: templates/rpc/IDL_FILE)\n"
		   COLOR_FLAG"    -p "
		   COLOR_WHITE": specify the path for idl_file to depend "
		   "(default: templates/rpc/)\n"
		   COLOR_OFF, name, name);
}

bool RPCController::copy_files()
{
	struct srpc_config *config = &this->config;
	int ret = true;
	
	if (config->specified_idl_file == NULL) // fill the default rpc files
	{
		this->fill_rpc_default_files();
	}
	else
	{
		// copy specified idl file and generate rpc files
		struct GeneratorParams params;
		params.out_dir = config->output_path;
		params.input_dir = config->specified_idl_path;
		if (params.input_dir[params.input_dir.length() - 1] != '/')
			params.input_dir += "/";

		params.idl_file = config->specified_idl_file;

		auto pos = params.idl_file.find_last_of('/');
		if (pos != std::string::npos)
			params.idl_file = params.idl_file.substr(pos + 1);

		std::string out_file = std::string(config->output_path) +
							   std::string("/") + params.idl_file;

		if (CommandController::copy_single_file(config->specified_idl_file,
												out_file, nullptr) == false)
			return false;

		ControlGenerator gen(config);
		printf(COLOR_PURPLE"Info: srpc generator begin.\n" COLOR_OFF);
		ret = gen.generate(params);

		if (ret == false)
		{
			printf(COLOR_RED"Error: srpc generator error.\n\n" COLOR_OFF);
			return false;
		}

		printf(COLOR_PURPLE"Info: srpc generator done.\n\n" COLOR_OFF);
	}

	return CommandController::copy_files();
}

bool RPCController::get_opt(int argc, const char **argv)
{
	struct srpc_config *config = &this->config;
	char c;
	optind = 3;

	while ((c = getopt(argc, (char * const *)argv,
					   "o:r:i:x:c:s:d:f:p:")) != -1)
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
		case 'd':
			config->specified_depend_path = true;
			memset(config->depend_path, 0, MAXPATHLEN);
			if (sscanf(optarg, "%s", config->depend_path) != 1)
				return false;
			break;
		case 'f':
			config->specified_idl_file = optarg;
			break;
		case 'p':
			config->specified_idl_path = optarg;
			break;
		default:
			printf(COLOR_RED "Error:\n     Unknown args : "
				   COLOR_BLUE "%s\n\n" COLOR_OFF, argv[optind - 1]);
			return false;
		}
	}

	return true;
}

bool RPCController::check_args()
{
	if (CommandController::check_args() == false)
		return false;

	struct srpc_config *config = &this->config;

	if (config->rpc_type == PROTOCOL_TYPE_MAX ||
		config->idl_type == IDL_TYPE_MAX ||
		config->data_type == DATA_TYPE_MAX ||
		config->compress_type == COMPRESS_TYPE_MAX)
	{
		printf(COLOR_RED"Error:\n      Invalid rpc args.\n" COLOR_OFF);
		return false;
	}

	switch (config->rpc_type)
	{
	case PROTOCOL_TYPE_THRIFT:
	case PROTOCOL_TYPE_THRIFT_HTTP:
		if (config->idl_type == IDL_TYPE_PROTOBUF ||
			config->data_type == DATA_TYPE_PROTOBUF)
		{
			printf(COLOR_RED"Error:\n      "
				   COLOR_BLUE"\" %s \" "
				   COLOR_RED"does NOT support protobuf as idl or data type.\n" COLOR_OFF,
				   config->rpc_type_string());
			return false;
		}
		if (config->idl_type == IDL_TYPE_DEFAULT)
			config->idl_type = IDL_TYPE_THRIFT; // as default;
		break;
	case PROTOCOL_TYPE_BRPC:
	case PROTOCOL_TYPE_TRPC:
	case PROTOCOL_TYPE_TRPC_HTTP:
		if (config->idl_type == IDL_TYPE_THRIFT ||
			config->data_type == DATA_TYPE_THRIFT)
		{
			printf(COLOR_RED "Error:\n      "
				   COLOR_BLUE "\" %s \" "
				   COLOR_RED "does NOT support thrift as idl or data type.\n" COLOR_OFF,
				   config->rpc_type_string());
			return false;
		}
	default:
		if (config->idl_type == IDL_TYPE_DEFAULT)
			config->idl_type = IDL_TYPE_PROTOBUF; // as default;
		break;
	}

	if (config->prepare_specified_idl_file() == false)
		return false;

	return true;
}

APIController::APIController()
{
	this->config.type = COMMAND_API;
	this->config.idl_type = IDL_TYPE_PROTOBUF;
}

void APIController::print_usage(const char *name) const
{
	printf(COLOR_PINK"Usage:\n"
		   COLOR_INFO"    %s " COLOR_COMMAND "api "
		   COLOR_INFO"<FILE_NAME> " COLOR_FLAG "[FLAGS]\n\n"
		   COLOR_PINK"Example:\n"
		   COLOR_PURPLE"    %s api my_api\n\n"
		   COLOR_PINK"Available Flags:\n"
		   COLOR_FLAG"    -o "
		   COLOR_WHITE": file output path (default: CURRENT_PATH)\n"
		   COLOR_FLAG"    -i "
		   COLOR_WHITE": idl type [ protobuf | thrift ] (default: protobuf)\n"
		   COLOR_OFF, name, name);
}

bool APIController::get_opt(int argc, const char **argv)
{
	char c;
	optind = 3;

	getcwd(this->config.output_path, MAXPATHLEN);

	while ((c = getopt(argc, (char * const *)argv, "o:i:")) != -1)
	{
		switch (c)
		{
		case 'o':
			memset(this->config.output_path, 0, MAXPATHLEN);
			if (sscanf(optarg, "%s", this->config.output_path) != 1)
				return false;
			break;
		case 'i':
			this->config.set_idl_type(optarg);
			break;
		default:
			printf(COLOR_RED "Error:\n     Unknown args : "
				   COLOR_BLUE "%s\n\n" COLOR_OFF, argv[optind - 1]);
			return false;
		}
	}

	return true;
}

bool APIController::check_args()
{
	if (*(this->config.project_name) == '-')
	{
		printf(COLOR_RED "Error: Invalid FILE_NAME\n\n" COLOR_OFF);
		return false;
	}

	return true;
}

bool APIController::dependencies_and_dir()
{
	std::string idl_file_name;
	std::string out_file_name = this->config.project_name;

	if (this->config.idl_type == IDL_TYPE_PROTOBUF)
	{
		out_file_name += ".proto";
		idl_file_name = "rpc/rpc.proto";
	}
	else if (this->config.idl_type == IDL_TYPE_THRIFT)
	{
		out_file_name += ".thrift";
		idl_file_name = "rpc/rpc.thrift";
	}

	std::string abs_file_name = this->config.output_path;
	if (abs_file_name.at(abs_file_name.length() - 1) != '/')
		abs_file_name += "/";
	abs_file_name += out_file_name;

	DIR *dir;
	dir = opendir(this->config.output_path);
	if (dir == NULL)
	{
		if (mkdir_p(this->config.output_path, 0755) != 0)
		{
			perror("Error:\n      failed to make output_path ");
			return false;
		}
	}
	else
	{
		closedir(dir);

		struct stat st;
		if (stat(abs_file_name.c_str(), &st) >= 0)
		{
			printf(COLOR_RED"Error:\n      file"
				   COLOR_BLUE" %s " COLOR_RED "EXISTED in path"
				   COLOR_BLUE" %s " COLOR_RED ".\n" COLOR_OFF,
				   out_file_name.c_str(), this->config.output_path);

			return false;
		}
	}

	dir = opendir(this->config.template_path);
	if (dir == NULL)
	{
		printf(COLOR_RED"Error:\n      template path "
			   COLOR_BLUE" %s " COLOR_RED "does NOT exist.\n" COLOR_OFF,
			   this->config.template_path);
		return false;
	}

	struct file_info info;
	info = { idl_file_name , out_file_name, rpc_idl_transform };
	this->default_files.push_back(info);

	closedir(dir);
	return true;
}

void APIController::print_success_info() const
{
	std::string file_name = this->config.project_name;

	if (this->config.idl_type == IDL_TYPE_PROTOBUF)
		file_name += ".proto";
	else if (this->config.idl_type == IDL_TYPE_THRIFT)
		file_name += ".thrift";

	printf(COLOR_GREEN"Success:\n      Create api file"
		   COLOR_BLUE" %s " COLOR_GREEN "at path"
		   COLOR_BLUE" %s " COLOR_GREEN "done.\n\n" COLOR_OFF,
		   file_name.c_str(), this->config.output_path);
	printf(COLOR_PINK"Suggestions:\n"
		   COLOR_WHITE"      Modify the api file as you needed.\n"
		   "      And make rpc project base on this file with the following command:\n\n"
		   COLOR_GREEN"      ./srpc rpc my_rpc_project -f %s -p %s\n\n" COLOR_OFF,
		   file_name.c_str(), this->config.output_path);
}

