#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#include "srpc_controller.h"

static constexpr const char *DEPENDENCIES_ERROR = 
"Warning: Default dependencies path : %s does not have \
Workflow or other third_party dependencies. \
This may cause link error in project : %s . \n\n\
Please check or specify srpc path with '-d' .\n\n\
Or use the following command to pull srpc:\n\
    \"git clone --recursive https://github.com/sogou/srpc.git\"\n\
    \"cd srpc && make\"\n\n";

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
	DIR *dir;

	dir = opendir(config->output_path);
	if (dir != NULL)
	{
		printf("Error:\n      project path \" %s \" EXISTS.\n\n",
				config->output_path);
		closedir(dir);

		return false;
	}

	dir = opendir(config->template_path);
	if (dir == NULL)
	{
		printf("Error:\n      template path \" %s \" does NOT exist.\n",
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
					printf("Error:\n       read \" %s \" failed\n\n", in_file.c_str());

				free(buf);
			}
			else
				printf("Error:\n      system error.\n\n");

			fclose(write_fp);
		}
		else
			printf("Error:\n       write \" %s \" failed\n\n", out_file.c_str());
	}
	else
		printf("Error:\n      open \" %s \" failed.\n\n", in_file.c_str());

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

void CommandController::print_success_info()
{
	printf("Success:\n      make project path \" %s \" done.\n\n",
			this->config.output_path);
	printf("Commands:\n      cd %s\n      make -j\n\n",
			this->config.output_path);
	printf("Execute:\n      ./server\n      ./client\n\n");
}

static bool http_cmake_transform(const std::string& format, FILE *out,
								 const struct srpc_config *config)
{
	std::string path = config->depend_path;
	path += "workflow";

	size_t len = fprintf(out, format.c_str(), config->project_name, path.c_str());

	return len > 0;
}

HttpController::HttpController()
{
	this->config.type = COMMAND_HTTP;
	struct file_info info;

	info = { "http/config.json", "config.json", nullptr };
	this->default_files.push_back(info);

	info = { "http/CMakeLists.txt", "CMakeLists.txt", http_cmake_transform };
	this->default_files.push_back(info);

	info = { "http/client_main.cc", "client_main.cc", nullptr };
	this->default_files.push_back(info);

	info = { "http/server_main.cc", "server_main.cc", nullptr };
	this->default_files.push_back(info);

	info = { "http/server_example.h", "server_example.h", nullptr };
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
			config->template_path = optarg; // TODO:
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

