/*
  Copyright (c) 2020 Sogou, Inc.

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

#ifndef __RPC_GENERATOR_H__
#define __RPC_GENERATOR_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <map>
#include <list>
#include <string>
#include <algorithm>

#include "printer.h"
#include "parser.h"

struct GeneratorParams
{
	const char *out_dir;
	const char *out_file;
};

class Generator
{
public:
	Generator(bool is_thrift):
		parser(is_thrift),
		printer(is_thrift)
	{
		this->suffix = ".srpc.";
		this->thrift_suffix = ".thrift.";
		this->skeleton_suffix = ".skeleton.";
		this->is_thrift = is_thrift;
	}

	bool generate(const std::string& idl_file, struct GeneratorParams params);

private:
	bool generate_header(idl_info& cur_info, struct GeneratorParams params);
	void generate_skeleton(const std::string& idl_file);

	bool generate_srpc_file(const idl_info& cur_info);
	bool generate_thrift_type_file(idl_info& cur_info);
	void generate_server_cpp_file(const idl_info& cur_info, const std::string& idle_file_name);
	void generate_client_cpp_file(const idl_info& cur_info, const std::string& idle_file_name);
	void thrift_replace_include(const idl_info& cur_info, std::vector<rpc_param>& params);

	bool init_file_names(const std::string& idl_file, const char *out_dir);

	Parser parser;
	Printer printer;
	idl_info info;
	std::string out_dir;
	std::string suffix;
	std::string thrift_suffix;
	std::string skeleton_suffix;
	std::string prefix;
	std::string srpc_file;
	std::string thrift_type_file;
	std::string server_cpp_file;
	std::string client_cpp_file;
	bool is_thrift;
};

#endif
