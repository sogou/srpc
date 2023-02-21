#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "srpc_config.h"

std::vector<std::string> RPC_PROTOC_SKIP_FILES =
{ "config_simple.h", "config_simple.cc",
  "rpc.thrift", "client_thrift.cc", "server_thrift.cc" };

std::vector<std::string> RPC_THRIFT_SKIP_FILES =
{ "config_simple.h", "config_simple.cc",
  "rpc.proto", "client_protobuf.cc", "server_protobuf.cc" };

void usage(int argc, const char *argv[])
{
	printf("Description:\n"
		   "    A handy generator for Workflow and SRPC project.\n\n"
		   "Usage:\n"
		   "    %s <COMMAND> <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Commands:\n"
		   "    \"http\"  - create project with both client and server\n"
		   "    \"rpc\"   - create project with both client and server\n"
		   "    \"redis\" - create redis client\n"
		   "    \"mysql\" - create mysql client\n"
		   "    \"kafka\" - create kafka client\n"
		   "    \"file\"  - create project with file service\n"
		   "    \"extra\" - TODO\n"
		   , argv[0]);
}

void usage_http(int argc, const char *argv[])
{
	printf("Usage:\n"
		   "    %s http <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Flags:\n"
		   "    -o :    project output path (default: CURRENT_PATH)\n"
		   "    -d :    path of dependencies (default: COMPILE_PATH)\n"
		   , argv[0]);
}

void usage_db(int argc, const char *argv[], const struct srpc_config *config)
{
	const char *name;
	unsigned short port;

	switch (config->type)
	{
	case COMMAND_REDIS:
		name = "redis";
		port = 6379;
		break;
	case COMMAND_MYSQL:
		name = "mysql";
		port = 3306;
		break;
	default:
		return;
	}

	printf("Usage:\n"
		   "    %s %s <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Flags:\n"
		   "    -o :    project output path (default: CURRENT_PATH)\n"
		   "    -h :    %s server url (default: %s://127.0.0.1:%u)\n"
		   "    -d :    path of dependencies (default: COMPILE_PATH)\n"
		   , argv[0], name, name, name, port);
}

void usage_kafka(int argc, const char *argv[])
{
	printf("Usage:\n"
		   "    %s kafka <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Flags:\n"
		   "    -o :    project output path (default: CURRENT_PATH)\n"
		   "    -h :    kafka broker url (example: kafka://10.160.23.23:9000,"
		   "10.123.23.23,kafka://kafka.sogou)\n"
		   "    -g :    group name\n"
		   "    -d :    path of dependencies (default: COMPILE_PATH)\n"
		   , argv[0]);
}

void usage_rpc(int argc, const char *argv[], const struct srpc_config *config)
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
		   , argv[0]);
}

// proto: 0; thrift: 1; error: -1;
static int check_file_idl_type(const char *filename)
{
	size_t len = strlen(filename);

	if (len > 6 && strcmp(filename + len - 6, ".proto") == 0)
		return 0;
	else if (len > 7 && strcmp(filename + len - 7, ".thrift") == 0)
		return 1;

	return -1;
}

srpc_config::srpc_config()
{
	rpc_type = RPC_TYPE_SRPC;
	idl_type = IDL_TYPE_DEFAULT;
	data_type = DATA_TYPE_DEFAULT;
	compress_type = COMPRESS_TYPE_NONE;
	specified_depend_path = false;
	specified_idl_file = NULL;
	specified_idl_path = NULL;
	template_path = TEMPLATE_PATH_DEFAULT;
}

bool srpc_config::prepare_dependencies() const
{
	if (this->specified_depend_path == true)
		return true;

	std::string workflow_file = this->depend_path;
	workflow_file += "workflow/workflow-config.cmake.in";
	if (access(workflow_file.c_str(), 0) != 0)
	{
		std::string cmd = "cd ";
		cmd += this->depend_path;
		cmd += "&& git submodule init && git submodule update";
		system(cmd.c_str());

		if (access(workflow_file.c_str(), 0) != 0)
		{
			printf("Warning: Default dependencies path : %s does not have "
					"Workflow or other third_party dependencies. "
					"This may cause link error in project : %s . \n\n"
					"Please check or specify srpc path with '-d' .\n\n"
					"Or use the following command to pull srpc:\n"
					"    \"git clone --recursive "
					"https://github.com/sogou/srpc.git\"\n"
					"    \"cd srpc && make\"\n\n",
					this->depend_path, this->project_name);
			return false;
		}
	}
	std::string srpc_lib = this->depend_path;
	srpc_lib += "_lib";

	if (access(srpc_lib.c_str(), 0) != 0)
	{
		std::string cmd = "cd ";
		cmd += this->depend_path;
		cmd += " && make -j4";
		system(cmd.c_str());
	}

	return true;
}

bool srpc_config::is_rpc_skip_file(const char *file_name) const
{
	size_t name_len = strlen(file_name);

	if (this->specified_idl_file == NULL)
	{
		std::vector<std::string>& v = RPC_PROTOC_SKIP_FILES;
		if (this->idl_type == IDL_TYPE_THRIFT)
			v = RPC_THRIFT_SKIP_FILES;

		for (size_t i = 0; i < v.size(); i++)
		{
			if (strncmp(file_name, v[i].data(), name_len) == 0)
				return true;
		}
	}
	else
	{
		for (auto &f : RPC_PROTOC_SKIP_FILES)
		{
			if (strncmp(file_name, f.data(), name_len) == 0)
				return true;
		}

		for (auto &f : RPC_THRIFT_SKIP_FILES)
		{
			if (strncmp(file_name, f.data(), name_len) == 0)
				return true;
		}
	}

	return false;
}

bool srpc_config::prepare_project_path()
{
	if (*this->project_name == '-')
		return false;

	size_t path_len = strlen(this->output_path);

	if (strlen(this->project_name) >= MAXPATHLEN - path_len - 2)
	{
		printf("Error:\n      project name \" %s \" is too long. limit %d.\n\n",
				this->project_name, MAXPATHLEN);
		return false;
	}

	snprintf(this->output_path + path_len, MAXPATHLEN - path_len, "/%s/",
			 this->project_name);

	return true;
}

bool srpc_config::prepare_args()
{
	if (this->prepare_project_path() == false)
		return false;

	if (this->type == COMMAND_RPC)
	{
		if (this->rpc_type == RPC_TYPE_MAX ||
			this->idl_type == IDL_TYPE_MAX ||
			this->data_type == DATA_TYPE_MAX ||
			this->compress_type == COMPRESS_TYPE_MAX)
		{
			printf("Error:\n      Invalid rpc args.\n");
			return false;
		}

		switch (this->rpc_type)
		{
		case RPC_TYPE_THRIFT:
		case RPC_TYPE_THRIFT_HTTP:
			if (this->idl_type == IDL_TYPE_PROTOBUF ||
				this->data_type == DATA_TYPE_PROTOBUF)
			{
				printf("Error:\n      "
					   "\" %s \" doesn`t support protobuf as idl or data type.\n",
					   this->rpc_type_string());
				return false;
			}
			if (this->idl_type == IDL_TYPE_DEFAULT)
				this->idl_type = IDL_TYPE_THRIFT; // as default;
			break;
		case RPC_TYPE_BRPC:
		case RPC_TYPE_TRPC:
		case RPC_TYPE_TRPC_HTTP:
			if (this->idl_type == IDL_TYPE_THRIFT ||
				this->data_type == DATA_TYPE_THRIFT)
			{
				printf("Error:\n      "
					   "\" %s \" does not support thrift as idl or data type.\n",
					   this->rpc_type_string());
				return false;
			}
		default:
			if (this->idl_type == IDL_TYPE_DEFAULT)
				this->idl_type = IDL_TYPE_PROTOBUF; // as default;
			break;
		}

		if (this->prepare_specified_idl_file() == false)
			return false;
	}

	return this->prepare_dependencies();
}

bool srpc_config::prepare_specified_idl_file()
{
	if (this->specified_idl_file == NULL)
	{
		if (this->specified_idl_path != NULL)
		{
			printf("Error:\n      idl_path is specified but NO idl_file.\n\n");
			return false;
		}

		return true;
	}

	if (check_file_idl_type(this->specified_idl_file) < 0)
	{
		printf("Error:\n      Invalid idl type. file : \" %s \"\n\n",
				this->specified_idl_file);
		return false;
	}

	if (access(this->specified_idl_file, F_OK) != 0)
	{
		printf("Error:\n      idl_file \" %s \" does NOT exist.\n\n",
				this->specified_idl_file);
		return false;
	}

	if (this->specified_idl_path == NULL)
		this->specified_idl_path = this->output_path;

	return true;
}

const char *srpc_config::rpc_type_string() const
{
	switch (this->rpc_type)
	{
	case RPC_TYPE_SRPC:
		return "SRPC";
	case RPC_TYPE_SRPC_HTTP:
		return "SRPCHttp";
	case RPC_TYPE_BRPC:
		return "BRPC";
	case RPC_TYPE_THRIFT:
		return "Thrift";
	case RPC_TYPE_THRIFT_HTTP:
		return "ThriftHttp";
	case RPC_TYPE_TRPC:
		return "TRPC";
	case RPC_TYPE_TRPC_HTTP:
		return "TRPCHttp";
	default:
		return "Unknown type";
	}
}

const char *srpc_config::rpc_compress_string() const
{
	switch (this->compress_type)
	{
	case COMPRESS_TYPE_SNAPPY:
		return "RPCCompressSnappy";
	case COMPRESS_TYPE_GZIP:
		return "RPCCompressGzip";
	case COMPRESS_TYPE_ZLIB:
		return "RPCCompressZlib";
	case COMPRESS_TYPE_LZ4:
		return "RPCCompressLz4";
	default:
		return "Unknown type";
	}
}

const char *srpc_config::rpc_data_string() const
{
	switch (this->data_type)
	{
	case DATA_TYPE_PROTOBUF:
		return "RPCDataProtobuf";
	case DATA_TYPE_THRIFT:
		return "RPCDataThrift";
	case DATA_TYPE_JSON:
		return "RPCDataJson";
	default:
		return "Unknown type";
	}
}

void srpc_config::set_rpc_type(const char *optarg)
{
	if (strcasecmp(optarg, "SRPC") == 0)
		this->rpc_type = RPC_TYPE_SRPC;
	else if (strcasecmp(optarg, "SRPCHttp") == 0)
		this->rpc_type = RPC_TYPE_SRPC_HTTP;
	else if (strcasecmp(optarg, "BRPC") == 0)
		this->rpc_type = RPC_TYPE_BRPC;
	else if (strcasecmp(optarg, "Thrift") == 0)
		this->rpc_type = RPC_TYPE_THRIFT;
	else if (strcasecmp(optarg, "ThriftHttp") == 0)
		this->rpc_type = RPC_TYPE_THRIFT_HTTP;
	else if (strcasecmp(optarg, "TRPC") == 0)
		this->rpc_type = RPC_TYPE_TRPC;
	else if (strcasecmp(optarg, "TRPC") == 0)
		this->rpc_type = RPC_TYPE_TRPC_HTTP;
	else
		this->rpc_type = RPC_TYPE_MAX;
}

void srpc_config::set_idl_type(const char *optarg)
{
	if (strcasecmp(optarg, "protobuf") == 0)
		this->idl_type = IDL_TYPE_PROTOBUF;
	else if (strcasecmp(optarg, "thrift") == 0)
		this->idl_type = IDL_TYPE_THRIFT;
	else
		this->idl_type = IDL_TYPE_MAX;
}

void srpc_config::set_data_type(const char *optarg)
{
	if (strcasecmp(optarg, "protobuf") == 0)
		this->data_type = DATA_TYPE_PROTOBUF;
	else if (strcasecmp(optarg, "thrift") == 0)
		this->data_type = DATA_TYPE_THRIFT;
	else if (strcasecmp(optarg, "json") == 0)
		this->data_type = DATA_TYPE_JSON;
	else
		this->data_type = DATA_TYPE_MAX;
}

void srpc_config::set_compress_type(const char *optarg)
{
	if (strcasecmp(optarg, "snappy") == 0)
		this->compress_type = COMPRESS_TYPE_SNAPPY;
	else if (strcasecmp(optarg, "gzip") == 0)
		this->compress_type = COMPRESS_TYPE_GZIP;
	else if (strcasecmp(optarg, "zlib") == 0)
		this->compress_type = COMPRESS_TYPE_ZLIB;
	else if (strcasecmp(optarg, "lz4") == 0)
		this->compress_type = COMPRESS_TYPE_LZ4;
	else
		this->compress_type = COMPRESS_TYPE_MAX;
}

ControlGenerator::ControlGenerator(const struct srpc_config *config) :
	Generator(config->idl_type == IDL_TYPE_THRIFT ? true : false),
	ctl_printer(config->idl_type == IDL_TYPE_THRIFT ? true : false),
	config(config)
{ }

bool ControlGenerator::generate_client_cpp_file(const idl_info& info,
												const std::string& idl_file)
{
	this->client_cpp_file = this->config->output_path;
	this->client_cpp_file += "client_main.cc";

	if (this->ctl_printer.open(this->client_cpp_file) == false)
		return false;

	this->ctl_printer.print_clt_include();
	this->ctl_printer.print_client_file_include(idl_file);

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		for (const auto& rpc : desc.rpcs)
		{
			this->ctl_printer.print_client_done_method_ctl(info.package_name,
														   desc.block_name,
														   rpc.method_name,
														   rpc.response_name);
		}
	}

	this->ctl_printer.print_client_main_begin();
	this->ctl_printer.print_load_config();
	this->ctl_printer.print_client_params();

	int id = 0;

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		std::string suffix;
		if (id != 0)
			suffix = std::to_string(id);
		id++;

		this->ctl_printer.print_client_main_service_ctl(this->config->rpc_type_string(),
														info.package_name,
														desc.block_name,
														suffix);

		auto rpc_it = desc.rpcs.cbegin();
		if (rpc_it != desc.rpcs.cend())
		{
			this->ctl_printer.print_client_main_method_call(info.package_name,
															desc.block_name,
															rpc_it->method_name,
															rpc_it->request_name,
															suffix);
			rpc_it++;
		}
	}

	this->ctl_printer.print_client_main_end();
	this->ctl_printer.close();

	return false;
}

bool ControlGenerator::generate_server_cpp_file(const idl_info& info,
												const std::string& idl_file)
{
	this->server_cpp_file = this->config->output_path;
	this->server_cpp_file += "server_main.cc";

	if (this->ctl_printer.open(this->server_cpp_file) == false)
		return false;

	this->ctl_printer.print_clt_include();
	this->ctl_printer.print_server_file_include(idl_file); 

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		this->ctl_printer.print_server_class_impl(info.package_name,
												  desc.block_name);

		for (const auto& rpc : desc.rpcs)
		{
			this->ctl_printer.print_server_impl_method(info.package_name,
													   desc.block_name,
													   rpc.method_name,
													   rpc.request_name,
													   rpc.response_name);
		}
		this->ctl_printer.print_server_class_impl_end();
	}

	this->ctl_printer.print_server_main_begin();
	this->ctl_printer.print_load_config();
	this->ctl_printer.print_server_construct(this->config->rpc_type_string());

	for (const auto& desc : info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		this->ctl_printer.print_server_main_method(desc.block_name);
	}

	this->ctl_printer.print_server_main_end_ctl(this->config->project_name,
											this->config->rpc_type_string());
	this->ctl_printer.print_server_main_return();
	this->ctl_printer.close();

	return true;	
}

static std::string ctl_include_format = R"(#include <stdio.h>
#include "config/config.h"
)";

static std::string ctl_load_config_format = R"(
	// load config
	srpc::RPCConfig config;
	if (config.load("./config.json") == false)
	{
		perror("Load config failed");
		exit(1);
	}

	signal(SIGINT, sig_handler);
)";

static std::string ctl_client_load_params_format = R"(
	// start client
	RPCClientParams params = RPC_CLIENT_PARAMS_DEFAULT;
	params.host = config.client_host();
	params.port = config.client_port();
)";

static std::string ctl_client_main_params_format = R"(
	%s::%sClient client%s(&params);
	config.load_filter(client%s);
)";

static std::string ctl_client_done_format = R"(
static void %s_done(%s *response, srpc::RPCContext *context)
{
	if (context->success())
		printf("%%s\n", response->DebugString().c_str());
	else
		printf("%s %s status[%%d] error[%%d] errmsg:%%s\n",
			   context->get_status_code(), context->get_error(),
			   context->get_errmsg());
}
)";

static std::string ctl_server_main_end_format = R"(
	config.load_filter(server);

	if (server.start(config.server_port()) == 0)
	{
		printf("%s %s server start, port %%u\n", config.server_port());
		wait_group.wait();
		server.stop();
	}
)";

void ControlGenerator::ControlPrinter::print_clt_include()
{
	fprintf(this->out_file, "%s", ctl_include_format.c_str());
}

void ControlGenerator::ControlPrinter::print_load_config()
{
	fprintf(this->out_file, "%s", ctl_load_config_format.c_str());
}

void ControlGenerator::ControlPrinter::print_client_params()
{
	fprintf(this->out_file, "%s", ctl_client_load_params_format.c_str());
	fprintf(this->out_file, "\tparams.is_ssl = config.client_is_ssl();\n");
	fprintf(this->out_file, "\tparams.url = config.client_url();\n");
	fprintf(this->out_file, "\tparams.caller = config.client_caller();\n");

	// TODO: set client task params
}

void ControlGenerator::ControlPrinter::
print_client_done_method_ctl(const std::vector<std::string>& package,
							 const std::string& service,
							 const std::string& method,
							 const std::string& response)
{
	std::string method_lower = method;
	std::transform(method_lower.begin(), method_lower.end(),
				   method_lower.begin(), ::tolower);

	std::string resp;

	if (this->is_thrift)
		resp = make_thrift_package_prefix(package, service, response);
	else
		resp = make_package_prefix(package, response);

	fprintf(this->out_file, ctl_client_done_format.c_str(),
			method_lower.c_str(), resp.c_str(),
			service.c_str(), method.c_str(), service.c_str(), method.c_str());
}

void ControlGenerator::ControlPrinter::
print_client_main_service_ctl(const std::string& type,
							  const std::vector<std::string>& package,
							  const std::string& service,
							  const std::string& suffix)
{
	std::string base_service = make_package_prefix(package, service);
	fprintf(this->out_file, ctl_client_main_params_format.c_str(),
			base_service.c_str(), type.c_str(),
			suffix.c_str(), suffix.c_str());
}

void ControlGenerator::ControlPrinter::
print_server_construct(const char *rpc_type)
{
	fprintf(this->out_file, "\t%sServer server;\n", rpc_type);
}

void ControlGenerator::ControlPrinter::
print_server_main_end_ctl(const char *project_name, const char *rpc_type)
{
	fprintf(this->out_file, ctl_server_main_end_format.c_str(),
			project_name, rpc_type);
}

