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

#ifndef __RPC_PARSER_H__
#define __RPC_PARSER_H__

#define PARSER_ST_BLOCK_MASK			0x01
#define PARSER_ST_OUTSIDE_BLOCK			0x00
#define PARSER_ST_INSIDE_BLOCK			0x01
#define PARSER_ST_OUTSIDE_BLOCK_MASK	0xFFFE	

#define PARSER_ST_COMMENT_MASK			0x10
#define PARSER_ST_INSIDE_COMMENT		0x10
#define PARSER_ST_OUTSIDE_COMMENT_MASK	0xFFFD
#define PARSER_ST_INSIDE_COMMENT_MASK	0xFFFF

#define DIRECTORY_LENGTH	2048

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <map>
#include <list>
#include <string>
#include <algorithm>
#include <unordered_map>

#include "descriptor.h"

class Parser
{
public:
	bool parse(const std::string& proto_file, idl_info& info);
	std::string find_typedef_mapping_type(std::string& type_name,
											size_t& cur, idl_info& info);
	void build_typedef_mapping(idl_info& info);
	int find_valid(const std::string& line);
	bool check_block_begin(FILE *in, std::string& line);
	bool check_block_begin(const std::string& line);
	bool check_block_end(const std::string& line);
	void check_single_comments(std::string& line);
	bool parse_block_name(const std::string& line,
						  std::string& block_name,
						  std::string& block_name_value,
						  std::string& extend_type);
	bool parse_service_pb(const std::string& block, Descriptor& desc);
	std::vector<std::string> split_thrift_rpc(const std::string &str);
	bool parse_thrift_typedef(const std::string& line,
							std::string& old_type_name,
							std::string& new_type_name,
							idl_info& info);
	bool parse_rpc_param_thrift(const std::string& file_name_prefix,
								const std::string &str,
								std::vector<rpc_param> &params,
								idl_info& info);
	bool parse_service_thrift(const std::string& file_name_prefix,
							  const std::string& block,
							  Descriptor& desc,
							  idl_info& info);
	bool parse_struct_thrift(const std::string& file_name_prefix,
							 const std::string& block,
							 Descriptor& desc, idl_info& info);
	bool parse_enum_thrift(const std::string& block, Descriptor& desc);
	bool parse_package_name(const std::string& line,
							std::vector<std::string>& package_name);
	bool parse_include_file(const std::string& line, std::string& file_name);
	bool check_multi_comments_begin(std::string& line);
	bool check_multi_comments_end(std::string& line);
	bool parse_dir_prefix(const std::string& file_name, char *dir_prefix);
	int parse_pb_rpc_option(const std::string& line);
	Parser(bool is_thrift) { this->is_thrift = is_thrift; }

private:
	bool is_thrift;
};

#endif

