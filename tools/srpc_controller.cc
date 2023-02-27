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

static constexpr const char *DEPENDENCIES_ERROR = 
"Warning: Default dependencies path : %s does not have \
Workflow or other third_party dependencies. \
This may cause link error in project : %s . \n\n\
Please check or specify srpc path with '-d' .\n\n\
Or use the following command to pull srpc:\n\
    \"git clone --recursive https://github.com/sogou/srpc.git\"\n\
    \"cd srpc && make\"\n\n";

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

static int mkdir_p(const char *name, mode_t mode)
{
	int ret = mkdir(name, mode);
	if (ret == 0 || errno != ENOENT)
		return ret;

	size_t len = strlen(name);
	if (len > MAXPATHLEN)
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	char path[MAXPATHLEN + 1] = {};
	memcpy(path, name, len);

	if (name[len - 1] != '/')
	{
		path[len] = '/';
		len++;
	}

	bool has_valid = false;

	for (int i = 0; i < len; i++)
	{
		if (path[i] != '/' && path[i] != '.') // simple check of valid path
		{
			has_valid = true;
			continue;
		}

		if (path[i] == '/' && has_valid == true)
		{
			path[i] = '\0';
			ret = mkdir(path, mode);
			if (ret != 0 && errno != EEXIST)
				return ret;

			path[i] = '/';
			has_valid = false;
		}
	}

	return ret;
}

bool CommandController::parse_args(int argc, const char **argv)
{
	if (argc < 3)
	{
		printf("Missing: PROJECT_NAME\n\n");
		return false;
	}

	getcwd(this->config.output_path, MAXPATHLEN);
	this->config.project_name = argv[2];
	this->config.service_name = argv[2];
	if (get_path(__FILE__, this->config.depend_path, 2) == false)
		return false;

	if (this->get_opt(argc, argv) == false)
		return false;

	if (this->check_args() == false)
		return false;

	return true;
}

bool CommandController::check_args()
{
	if (*(this->config.project_name) == '-')
		return false;

	size_t path_len = strlen(this->config.output_path);

	if (strlen(this->config.project_name) >= MAXPATHLEN - path_len - 2)
	{
		printf("Error:\n      project name \" %s \" or path \" %s \" "\
               " is too long. total limit %d.\n\n",
			   this->config.project_name, this->config.output_path, MAXPATHLEN);
		return false;
	}

	snprintf(this->config.output_path + path_len, MAXPATHLEN - path_len, "/%s/",
			 this->config.project_name);

	return true;
}

bool CommandController::dependencies_and_dir()
{
	struct srpc_config *config = &this->config;
	
	if (opendir(config->output_path) != NULL)
	{
		printf("Error:\n      project path \" %s \" EXISTS.\n\n",
				config->output_path);
		return false;
	}

	if (opendir(config->template_path) == NULL)
	{
		printf("Error:\n      template path \" %s \" does NOT exist.\n",
				config->template_path);
		return false;
	}

	if (config->specified_depend_path == false)
	{
		std::string workflow_file = config->depend_path;
		workflow_file += "workflow/workflow-config.cmake.in";
		if (access(workflow_file.c_str(), 0) != 0)
		{
			std::string cmd = "cd ";
			cmd += config->depend_path;
			cmd += "&& git submodule init && git submodule update";
			system(cmd.c_str());

			if (access(workflow_file.c_str(), 0) != 0)
			{
				printf(DEPENDENCIES_ERROR, config->depend_path,
						config->project_name);
				return false;
			}
		}

		std::string srpc_lib = config->depend_path;
		srpc_lib += "_lib";

		if (access(srpc_lib.c_str(), 0) != 0)
		{
			std::string cmd = "cd ";
			cmd += config->depend_path;
			cmd += " && make -j4";
			system(cmd.c_str());
		}
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
	printf("Commands:\n      cd %s\n      make -j\n\n", config->output_path);
	printf("Execute:\n      ./server\n      ./client\n\n");

	return true;
}

bool CommandController::copy_template()
{
	return this->copy_files();
}

// get the depth-th upper path of file
bool CommandController::get_path(const char *file, char *path, int depth)
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

bool CommandController::get_opt(int argc, const char **argv)
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
			config->template_path = optarg; // TODO: 
			break;
		case 'd':
			config->specified_depend_path = true;
			memset(config->depend_path, 0, MAXPATHLEN);
			if (sscanf(optarg, "%s", config->depend_path) != 1)
				return false;
		default:
//			printf("Error:\n     Unknown args : %s\n\n", argv[optind - 1]);
//			return false;
			break;//TODO: this is not very correct. better check themselves
		}
	}

	return true;
}



bool RPCController::get_opt(int argc, const char **argv)
{
	if (CommandController::get_opt(argc, argv) == false)
		return false;

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

bool CommandController::copy_path_files(const char *path)
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
					if (this->generate_from_template(entry->d_name, buf, size))
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

	printf("[1412] copy_path_file() left_file_count = %d\n", left_file_count);
	return left_file_count == 0;
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

bool HttpController::copy_files()
{
	struct srpc_config *config = &this->config;

	std::string path = config->template_path;
	path += "/common/";
	if (CommandController::copy_path_files(path.c_str()) == false)
		return false;

	path = config->template_path;
	path += "/config/";
	if (CommandController::copy_path_files(path.c_str()) == false)
		return false;

	path = config->template_path;
	path += "/http/";
	if(CommandController::copy_path_files(path.c_str()) == false)
		return false;

	return true;
}

bool HttpController::generate_from_template(const char *file_name,
											char *format,
											size_t size)
{
	FILE *write_fp;
	struct srpc_config *config = &this->config;
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

	std::string path = config->template_path;
	path += "/common/";
	if (CommandController::copy_path_files(path.c_str()) == false)
		return false;

	path = config->template_path;
	path += "/config/";
	if (CommandController::copy_path_files(path.c_str()) == false)
		return false;

	path = config->template_path;
	path += "/rpc/";
	if (CommandController::copy_path_files(path.c_str()) == false)
		return false;

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

 		// TODO: copy relevant proto files
		if (CommandController::copy_path_files(idl_path) == false)
			return false;

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

	return true;
}

bool RPCController::generate_from_template(const char *file_name,
										   char *format,
										   size_t size)
{
	size_t len;
	struct srpc_config *config = &this->config;
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
	{
		return true;
	}
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
	return len != 0;
}

/*
int DBController::parse(int argc, const char *argv[])
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
	return 0;
}

int KafkaController::parse(int argc, const char *argv[])
{
	config->type = COMMAND_KAFKA;
	config->server_url = "kafka://127.0.0.1:9092";
	if (parse_args(argc, argv, config) == false)
	{
		usage_kafka(argc, argv);
		return -1;
	}

	// copy files
	return 0;
}

int ExtraController::parse(int argc, const char *argv[])
{
	return 0;
}
*/
