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
	printf("Usage:\n"
		   "    %s rpc <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Flags:\n"
		   "    -r :    rpc type [ SRPC | SRPCHttp | BRPC | Thrift | "
		   "ThriftHttp | TRPC | TRPCHttp ] (default: SRPC)\n"
		   "    -o :    project output path (default: CURRENT_PATH)\n"
		   "    -s :    service name (default: PROJECT_NAME)\n"
		   "    -i :    idl type [ protobuf | thrift ] (default: protobuf)\n"
		   "    -x :    data type [ protobuf | thrift | json ] "
		   "(default: idl type. json for http)\n"
		   "    -c :    compress type [ gzip | zlib | snappy | lz4 ] "
		   "(default: no compression)\n"
		   "    -d :    path of dependencies (default: COMPILE_PATH)\n"
		   "    -f :    specify the idl_file to generate codes "
		   "(default: templates/rpc/IDL_FILE)\n"
		   "    -p :    specify the path for idl_file to depend "
		   "(default: templates/rpc/)\n"
		   , name);
}

bool RPCController::copy_files()
{
	struct srpc_config *config = &this->config;
	
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
		printf("Info: srpc-ctl generator begin.\n");
		gen.generate(params);
		printf("Info: srpc-ctl generator done.\n\n");
	}

	return CommandController::copy_files();
}

bool RPCController::get_opt(int argc, const char **argv)
{
	struct srpc_config *config = &this->config;
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
			if (sscanf(optarg, "%s", config->template_path) != 1)
				return false; //TODO:
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
		printf("Error:\n      Invalid rpc args.\n");
		return false;
	}

	switch (config->rpc_type)
	{
	case PROTOCOL_TYPE_THRIFT:
	case PROTOCOL_TYPE_THRIFT_HTTP:
		if (config->idl_type == IDL_TYPE_PROTOBUF ||
			config->data_type == DATA_TYPE_PROTOBUF)
		{
			printf("Error:\n      "
				   "\" %s \" does NOT support protobuf as idl or data type.\n",
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
			printf("Error:\n      "
				   "\" %s \" does NOT support thrift as idl or data type.\n",
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

