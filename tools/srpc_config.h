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

#ifndef __SRPC_CONFIG__
#define __SRPC_CONFIG__

#include <stdint.h>
#include <generator/generator.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/param.h>
#else
#define MAXPATHLEN 4096
#include <direct.h>
#endif

struct srpc_config
{
	uint16_t type;
	uint8_t rpc_type;
	uint8_t idl_type;
	uint8_t data_type;
	uint8_t compress_type;
	bool specified_depend_path;
	const char *specified_idl_file;
	const char *specified_idl_path;
	const char *project_name;
	const char *server_url;
	const char *service_name;
	const char *template_path;
	char depend_path[MAXPATHLEN];
	char output_path[MAXPATHLEN];

	srpc_config();

	bool prepare_args();
	bool prepare_project_path();
	bool prepare_dependencies() const;
	bool prepare_specified_idl_file();
	const char *rpc_type_string() const;
	const char *rpc_compress_string() const;
	const char *rpc_data_string() const;
	void set_rpc_type(const char *optarg);
	void set_idl_type(const char *optarg);
	void set_data_type(const char *optarg);
	void set_compress_type(const char *optarg);
	bool is_rpc_skip_file(const char *file_name) const;
};

class ControlGenerator : public Generator
{
public:
	bool generate_client_cpp_file(const idl_info& cur_info,
								  const std::string& idl_file_name);
	bool generate_server_cpp_file(const idl_info& cur_info,
								  const std::string& idl_file_name);

	ControlGenerator(const struct srpc_config *config);

private:
	class ControlPrinter : public Printer
	{
	public:
		ControlPrinter(bool is_thrift) : Printer(is_thrift) { }

		void print_clt_include();
		void print_load_config();

		void print_client_params();
		void print_client_done_method_ctl(const std::vector<std::string>& pkg,
										  const std::string& service,
										  const std::string& method,
										  const std::string& response);
		void print_client_main_service_ctl(const std::string& type,
										   const std::vector<std::string>& pkg,
										   const std::string& service,
										   const std::string& suffix);

		void print_server_construct(const char *rpc_type);
		void print_server_main_end_ctl(const char *project_name,
									   const char *rpc_type);
	};

	ControlPrinter ctl_printer;
	const struct srpc_config *config;
};

enum
{
	COMMAND_HTTP,
	COMMAND_REDIS,
	COMMAND_MYSQL,
	COMMAND_KAFKA,
	COMMAND_RPC,
	COMMAND_FILE,
	COMMAND_EXTRA
};

enum
{
	RPC_TYPE_SRPC,
	RPC_TYPE_SRPC_HTTP,
	RPC_TYPE_BRPC,
	RPC_TYPE_THRIFT,
	RPC_TYPE_THRIFT_HTTP,
	RPC_TYPE_TRPC,
	RPC_TYPE_TRPC_HTTP,
	RPC_TYPE_MAX
};

enum
{
	IDL_TYPE_DEFAULT,
	IDL_TYPE_PROTOBUF,
	IDL_TYPE_THRIFT,
	IDL_TYPE_MAX
};

enum
{
	DATA_TYPE_DEFAULT,
	DATA_TYPE_PROTOBUF,
	DATA_TYPE_THRIFT,
	DATA_TYPE_JSON,
	DATA_TYPE_MAX
};

enum
{
	COMPRESS_TYPE_NONE,
	COMPRESS_TYPE_SNAPPY,
	COMPRESS_TYPE_GZIP,
	COMPRESS_TYPE_ZLIB,
	COMPRESS_TYPE_LZ4,
	COMPRESS_TYPE_MAX
};

void usage(int argc, const char *argv[]);
void usage_http(int argc, const char *argv[]);
void usage_db(int argc, const char *argv[], const struct srpc_config *config);
void usage_kafka(int argc, const char *argv[]);
void usage_rpc(int argc, const char *argv[], const struct srpc_config *config);

#endif

