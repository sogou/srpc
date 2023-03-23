#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#include "srpc_controller.h"

static constexpr const char *DEPENDENCIES_ERROR = R"(Warning:
Default dependencies path : %s does not have
Workflow or other third_party dependencies.
This may cause link error in project : %s

Please check or specify srpc path with '-d'

Or use the following command to pull srpc:
    "git clone --recursive https://github.com/sogou/srpc.git"
    "cd srpc && make"
)";

static constexpr const char *CMAKE_PROTOC_CODES = R"(
find_program(PROTOC "protoc")
if(${PROTOC} STREQUAL "PROTOC-NOTFOUND")
    message(FATAL_ERROR "Protobuf compiler is missing!")
endif ()
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${IDL_FILE}))";

int mkdir_p(const char *name, mode_t mode)
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

static const char *get_proxy_rpc_string(const char *type)
{
	if (strcasecmp(type, "SRPCHttp") == 0)
		return "SRPCHttp";
	if (strcasecmp(type, "SRPC") == 0)
		return "SRPC";
	if (strcasecmp(type, "BRPC") == 0)
		return "BRPC";
	if (strcasecmp(type, "ThriftHttp") == 0)
		return "ThriftHttp";
	if (strcasecmp(type, "Thrift") == 0)
		return "Thrift";
	if (strcasecmp(type, "TRPCHttp") == 0)
		return "TRPCHttp";
	if (strcasecmp(type, "TRPC") == 0)
		return "TRPC";

	return "Unknown type";
}

// path=/root/a/
// file=b/c/d.txt
// make the middle path and return the full file name
static std::string make_file_path(const char *path, const std::string& file)
{
	DIR *dir;
	auto pos = file.find_last_of('/');
	std::string file_name;

	if (pos != std::string::npos)
	{
		file_name = file.substr(pos + 1);
		std::string dir_name = std::string(path) + std::string("/") +
							   file.substr(0, pos + 1);

		dir = opendir(dir_name.c_str());
		if (dir == NULL)
		{
			if (mkdir_p(dir_name.c_str(), 0755) != 0)
				return "";
		}
		else
			closedir(dir);
	}

	file_name = path;
	file_name += "/";
	file_name += file;

	return file_name;
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
		ret += "        ctx->set_compress_type(";
		ret += config->rpc_compress_string();
		ret += ");\n";
	}

	if (config->data_type != DATA_TYPE_DEFAULT)
	{
		ret += "        ctx->set_data_type(";
		ret += config->rpc_data_string();
		ret += ");\n";
	}

	ret += "        ";

	return ret;
}

bool rpc_idl_transform(const std::string& format, FILE *out,
					   const struct srpc_config *config)
{
	size_t len = fprintf(out, format.c_str(), config->service_name);

	return len > 0;
}

bool rpc_client_transform(const std::string& format, FILE *out,
						  const struct srpc_config *config)
{
	std::string prepare_params = rpc_client_file_codes(config);

	const char *rpc_type;
	if (config->type == COMMAND_PROXY)
		rpc_type = get_proxy_rpc_string(config->proxy_client_type_string());
	else
		rpc_type = config->rpc_type_string();

	const char *thrift_namespace = "";
	if (config->idl_type == IDL_TYPE_THRIFT)
		thrift_namespace = config->project_name;

	size_t len = fprintf(out, format.c_str(),
						 config->service_name, prepare_params.c_str(),
						 config->service_name, rpc_type,
						 thrift_namespace, thrift_namespace);
	return len > 0;
}

bool rpc_server_transform(const std::string& format, FILE *out,
						  const struct srpc_config *config)
{
	std::string prepare_ctx = rpc_server_file_codes(config);

	const char *rpc_type;
	if (config->type == COMMAND_PROXY)
		rpc_type = get_proxy_rpc_string(config->proxy_server_type_string());
	else
		rpc_type = config->rpc_type_string();

	size_t len = fprintf(out, format.c_str(),
						 config->service_name, config->service_name,
						 prepare_ctx.c_str(), rpc_type,
						 config->project_name, rpc_type);

	return len > 0;
}

bool CommandController::parse_args(int argc, const char **argv)
{
	if (argc < 3)
	{
		if (argc == 2 && strncmp(argv[1], "api", strlen(argv[1])) == 0)
			printf(COLOR_RED "Missing: FILE_NAME\n\n" COLOR_OFF);
		else
			printf(COLOR_RED "Missing: PROJECT_NAME\n\n" COLOR_OFF);

		return false;
	}

	memset(this->config.output_path, 0, MAXPATHLEN);

	if (get_path(__FILE__, this->config.depend_path, 2) == false)
		return false;

	if (get_path(__FILE__, this->config.template_path, 1) == false)
		return false;

	snprintf(this->config.template_path + strlen(this->config.template_path),
			 MAXPATHLEN - strlen(this->config.template_path), "templates/");

	if (this->get_opt(argc, argv) == false)
		return false;

	if (this->check_args() == false)
		return false;

	return true;
}

bool CommandController::check_args()
{
	size_t path_len = strlen(this->config.output_path);

	if (strlen(this->config.project_name) >= MAXPATHLEN - path_len - 2)
	{
		printf(COLOR_RED"Error:\n      project name" COLOR_BLUE " %s "
			   COLOR_RED"or path" COLOR_BLUE " %s "
               COLOR_RED" is too long. Total limit : %d.\n\n" COLOR_OFF,
			   this->config.project_name, this->config.output_path, MAXPATHLEN);
		return false;
	}

	if (path_len != 0 && this->config.output_path[path_len - 1] != '/')
	{
		this->config.output_path[path_len] = '/';
		path_len++;
	}

	snprintf(this->config.output_path + path_len, MAXPATHLEN - path_len, "%s/",
			 this->config.project_name);

	return true;
}

bool CommandController::dependencies_and_dir()
{
	struct srpc_config *config = &this->config;
	DIR *dir;
	int status;

	dir = opendir(config->output_path);
	if (dir != NULL)
	{
		printf(COLOR_RED "Error:\n      project path "
			   COLOR_BLUE " %s " COLOR_RED " EXISTS.\n\n" COLOR_OFF,
			   config->output_path);

		closedir(dir);
		return false;
	}

	dir = opendir(config->template_path);
	if (dir == NULL)
	{
		printf(COLOR_RED "Error:\n      template path "
			   COLOR_BLUE " %s " COLOR_RED "does NOT exist.\n" COLOR_OFF,
			   config->template_path);
		return false;
	}
	closedir(dir);

	if (config->specified_depend_path == false)
	{
		std::string workflow_file = config->depend_path;
		workflow_file += "workflow/workflow-config.cmake.in";
		if (access(workflow_file.c_str(), 0) != 0)
		{
			std::string cmd = "cd ";
			cmd += config->depend_path;
			cmd += "&& git submodule init && git submodule update";

			status = system(cmd.c_str());
			if (status == -1 || status == 127)
			{
				perror("git submodule init failed");
				return false;
			}

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

			status = system(cmd.c_str());
			if (status == -1 || status == 127)
			{
				perror("execute command make failed");
				return false;
			}
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

	return true;
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

bool CommandController::copy_single_file(const std::string& in_file,
										 const std::string& out_file,
										 transform_function_t transform)
{
	FILE *read_fp;
	FILE *write_fp;
	size_t size;
	char *buf;
	bool ret = false;

	read_fp = fopen(in_file.c_str(), "r");
	if (read_fp)
	{
		write_fp = fopen(out_file.c_str(), "w");
		if (write_fp)
		{
			fseek(read_fp, 0, SEEK_END);
			size = ftell(read_fp);

			buf = (char *)malloc(size);
			if (buf)
			{
				fseek(read_fp, 0, SEEK_SET);

				if (fread(buf, size, 1, read_fp) == 1)
				{
					if (transform != nullptr)
					{
						std::string format = std::string(buf, size);
						ret = transform(format, write_fp, &this->config);
					}
					else
						ret = fwrite(buf, size, 1, write_fp) >= 0 ? true : false;
				}
				else
				{
					printf(COLOR_RED"Error:\n       read " COLOR_WHITE
						   " %s " COLOR_RED "failed\n\n" COLOR_OFF,
						   in_file.c_str());
				}

				free(buf);
			}
			else
				printf(COLOR_RED"Error:\n      system error.\n\n" COLOR_OFF);

			fclose(write_fp);
		}
		else
		{
			printf(COLOR_RED"Error:\n       write" COLOR_WHITE " %s "
				   COLOR_RED"failed\n\n" COLOR_OFF, out_file.c_str());
		}
	}
	else
	{
		printf(COLOR_RED"Error:\n      open" COLOR_WHITE " %s "
			   COLOR_RED " failed.\n\n" COLOR_OFF, in_file.c_str());
	}
	return ret;
}

bool CommandController::copy_files()
{
	std::string read_file;
	std::string write_file;
	bool ret = true;

	for (auto it = this->default_files.begin();
		 it != this->default_files.end() && ret == true; it++)
	{
		read_file = this->config.template_path;
		read_file += it->in_file;

		write_file = make_file_path(this->config.output_path, it->out_file);

		if (write_file.empty())
			ret = false;
		else
			ret = this->copy_single_file(read_file, write_file, it->transform);
	}

	return ret;
}

void CommandController::print_success_info() const
{
	printf(COLOR_GREEN"Success!\n      make project path"
		   COLOR_BLUE" %s " COLOR_GREEN " done.\n\n",
		   this->config.output_path);
	printf(COLOR_PINK"Commands:\n      " COLOR_BLUE "cd %s\n      make -j\n\n",
		   this->config.output_path);
	printf(COLOR_PINK"Execute:\n      "
		   COLOR_GREEN "./server\n      ./client\n\n" COLOR_OFF);
}

bool common_cmake_transform(const std::string& format, FILE *out,
							const struct srpc_config *config)
{
	std::string path = config->depend_path;
	path += "workflow";

	std::string codes_str;
	std::string executors_str;

	if (config->type == COMMAND_FILE)
		codes_str = " file_service.cc";

	if (config->type != COMMAND_FILE && config->type != COMMAND_COMPUTE)
		executors_str = " client";

	if (config->type == COMMAND_PROXY)
		executors_str += " proxy";

	size_t len = fprintf(out, format.c_str(), config->project_name,
						 path.c_str(), codes_str.c_str(), executors_str.c_str());

	return len > 0;
}

void CommandController::fill_rpc_default_files()
{
	struct file_info info;
	std::string idl_file_name, client_file_name, server_file_name;
	std::string out_file_name = this->config.project_name;

	if (this->config.idl_type == IDL_TYPE_PROTOBUF)
	{
		out_file_name += ".proto";
		idl_file_name = "rpc/rpc.proto";
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

	info = { "rpc/server.conf", "server.conf", nullptr };
	this->default_files.push_back(info);

	if (this->config.type == COMMAND_RPC)
		info = { "rpc/client.conf", "client.conf", nullptr };
	else
		info = { "proxy/client_rpc.conf", "client.conf", nullptr };
	this->default_files.push_back(info);
}

bool rpc_cmake_transform(const std::string& format, FILE *out,
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

	std::string is_proxy_str = config->type == COMMAND_PROXY ? " proxy" : "";

	size_t len = fprintf(out, format.c_str(), config->project_name,
						 workflow_path.c_str(), srpc_path.c_str(),
						 idl_file_name.c_str(),
						 config->idl_type == IDL_TYPE_PROTOBUF ?
						 CMAKE_PROTOC_CODES : "",
						 is_proxy_str.c_str());

	return len > 0;
}

