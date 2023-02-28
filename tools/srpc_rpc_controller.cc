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

static constexpr const char *CMAKE_PROTOC_CODES = R"(
find_program(PROTOC "protoc")
if(${PROTOC} STREQUAL "PROTOC-NOTFOUND")
    message(FATAL_ERROR "Protobuf compiler is missing!")
endif ()
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${IDL_FILE}))";

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
static bool rpc_cmake_transform(const std::string& format, FILE *out,
								const struct srpc_config *config)
{
	std::string idl_file_name;

	std::string srpc_path = config->depend_path;
	std::string workflow_path = config->depend_path;
	workflow_path += "workflow";

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

	size_t len = fprintf(out, format.c_str(), config->project_name,
						 workflow_path.c_str(), srpc_path.c_str(),
						 idl_file_name.c_str(),
						 config->idl_type == IDL_TYPE_PROTOBUF ?
						 CMAKE_PROTOC_CODES : "");

	return len > 0;
}

static bool rpc_idl_transform(const std::string& format, FILE *out,
							  const struct srpc_config *config)
{
	size_t len = fprintf(out, format.c_str(), config->service_name);

	return len > 0;
}

static bool rpc_client_transform(const std::string& format, FILE *out,
								 const struct srpc_config *config)
{
	std::string prepare_params = rpc_client_file_codes(config);
	size_t len = fprintf(out, format.c_str(),
						 config->service_name, prepare_params.c_str(),
						 config->service_name, config->rpc_type_string());
	return len > 0;
}

static bool rpc_server_transform(const std::string& format, FILE *out,
								 const struct srpc_config *config)
{
	std::string prepare_ctx = rpc_server_file_codes(config);
	size_t len = fprintf(out, format.c_str(),
						 config->service_name, config->service_name,
						 config->service_name, prepare_ctx.c_str(),
						 config->rpc_type_string(), config->service_name,
						 config->project_name, config->rpc_type_string());

	return len > 0;
}

RPCController::RPCController()
{
	this->config.type = COMMAND_RPC;
	struct file_info info;

	info = { "rpc/CMakeLists.txt", "CMakeLists.txt", rpc_cmake_transform };
	this->default_files.push_back(info);

	info = { "common/config.json", "config.json", nullptr };
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
		   "(default: template/rpc/IDL_FILE)\n"
		   "    -p :    specify the path for idl_file to depend "
		   "(default: template/rpc/)\n"
		   , name);
}

bool RPCController::copy_files()
{
	struct srpc_config *config = &this->config;
	
	if (config->specified_idl_file == NULL) // fill the default rpc files
	{
		struct file_info info;
		std::string idl_file_name, client_file_name, server_file_name;
		std::string out_file_name = config->project_name;

		if (config->idl_type == IDL_TYPE_PROTOBUF)
		{
			out_file_name += ".proto";
			idl_file_name = "rpc/rpc.thrift";
			client_file_name = "rpc/client_protobuf.cc";
			server_file_name = "rpc/server_protobuf.cc";
		}
		else
		{
			out_file_name += ".thrift";
			idl_file_name = "rpc/rpc.thrift";
			client_file_name = "rpc/client_thrift.cc";
			server_file_name = "rpc/server_thrift.cc";
		}

		info = { idl_file_name , out_file_name, rpc_idl_transform };
		this->default_files.push_back(info);

		info = { client_file_name, "client_main.cc", rpc_client_transform };
		this->default_files.push_back(info);

		info = { server_file_name, "server_main.cc", rpc_server_transform };
		this->default_files.push_back(info);
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

	return true;
}

bool RPCController::check_args()
{
	if (CommandController::check_args() == false)
		return false;

	struct srpc_config *config = &this->config;

	if (config->rpc_type == RPC_TYPE_MAX ||
		config->idl_type == IDL_TYPE_MAX ||
		config->data_type == DATA_TYPE_MAX ||
		config->compress_type == COMPRESS_TYPE_MAX)
	{
		printf("Error:\n      Invalid rpc args.\n");
		return false;
	}

	switch (config->rpc_type)
	{
	case RPC_TYPE_THRIFT:
	case RPC_TYPE_THRIFT_HTTP:
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
	case RPC_TYPE_BRPC:
	case RPC_TYPE_TRPC:
	case RPC_TYPE_TRPC_HTTP:
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
