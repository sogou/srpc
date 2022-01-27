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

#include <ctype.h>
#include <unistd.h>
#include <sys/param.h>
#include "parser.h"
#include "thrift/rpc_thrift_enum.h"

#define LINE_LENGTH_MAX 2048

static std::string gen_param_var(const std::string& type_name, size_t& cur,
								 idl_info& info);
std::string type_prefix_to_namespace(const std::string& type_name,
									 idl_info& info);
Descriptor *search_cur_file_descriptor(idl_info& info,
									   const std::string& block_type,
									   const std::string& block_name);
Descriptor *search_include_file_descriptor(idl_info& info,
										   const std::string include_file_name,
										   const std::string& block_type,
										   const std::string& block_name);
idl_info *search_include_file(idl_info& info, const std::string file_name);
idl_info *search_namespace(idl_info& info, const std::string name_space);
void parse_thrift_type_name(const std::string& type_name,
									std::string& type_prefix,
									std::string& real_type_name);

bool Parser::parse(const std::string& proto_file, idl_info& info)
{
	char current_dir[MAXPATHLEN] = {};
	std::string dir_prefix;

	auto pos = proto_file.find_last_of('/');
	if (pos == std::string::npos)
		info.file_name = proto_file;
	else
	{
		info.file_name = proto_file.substr(pos + 1);

		getcwd(current_dir, MAXPATHLEN);
		dir_prefix = current_dir;
		dir_prefix += "/";
	}

	pos = info.file_name.find_last_of('.');
	if (pos == std::string::npos)
		info.file_name_prefix = info.file_name + ".";
	else
		info.file_name_prefix = info.file_name.substr(0, pos + 1);

	info.absolute_file_path = proto_file;

	FILE *in = fopen(proto_file.c_str(), "r");
	if (!in)
	{
		fprintf(stderr, "[Parser] proto file: [%s] not exists.\n",
				proto_file.c_str());
		return false;
	}

	fprintf(stdout, "proto file: [%s]\n", proto_file.c_str());

	char line_buffer[LINE_LENGTH_MAX];
	std::string file_path;
	std::string block;
	int state = PARSER_ST_OUTSIDE_BLOCK;
	bool flag_append;
	int stack_count = 0;
	std::string block_name;
	std::string block_type;
	std::string extends_type;
	std::string old_type_name;
	std::string new_type_name;
	std::string thrift_type_prefix;
	std::string thrift_type_name;
	bool succ;

	while (fgets(line_buffer, LINE_LENGTH_MAX, in))
	{
		std::string line = SGenUtil::lstrip(line_buffer);

		if (line.empty())
			continue;

		this->check_single_comments(line);
		if (line.empty())
			continue;

		this->check_single_comments(line);
		if ((state & PARSER_ST_COMMENT_MASK) == PARSER_ST_INSIDE_COMMENT)
		{
			if (this->check_multi_comments_end(line))
			{
				state -= PARSER_ST_INSIDE_COMMENT;
				//	state |= PARSER_ST_OUTSIDE_COMMENT_MASK;
				if (line.empty())
					continue;
			}
			else
				continue;

		}
		else if (this->check_multi_comments_begin(line))
			state = (state & PARSER_ST_OUTSIDE_COMMENT_MASK) + PARSER_ST_INSIDE_COMMENT;

		if (this->is_thrift == false)
		{
			int rpc_option = this->parse_pb_rpc_option(line);
			if (rpc_option == 1)
				continue;
			else if (rpc_option == 2)
			{
				fprintf(stderr, "[Parser ERROR] %s must not set "
						"\"option cc_generic_services = true\" for srpc.\n",
						proto_file.c_str());
				return false;
			}
		}

		if (this->parse_package_name(line, info.package_name) == true)
			continue;

		if (this->parse_include_file(line, file_path) == true)
		{
			if (!this->is_thrift)
			{
				if (file_path.rfind("google/protobuf/", 0) == 0)
					continue;
			}

			file_path = dir_prefix + file_path;

			info.include_list.resize(info.include_list.size() + 1);
			succ = this->parse(file_path, info.include_list.back());
			if (!succ)
			{
				info.include_list.pop_back();
				return false;
			}

			continue;
		}

		if (this->is_thrift &&
			this->parse_thrift_typedef(line,
									   old_type_name,
									   new_type_name,info) == true)
		{
			info.typedef_list.push_back(typedef_descriptor{old_type_name,
														   new_type_name});
			continue;
		}

		flag_append = false;

		if ((state & PARSER_ST_BLOCK_MASK) == PARSER_ST_OUTSIDE_BLOCK)
		{
			if (this->check_block_begin(in, line) == true)
			{
				state |= PARSER_ST_INSIDE_BLOCK;
//				stack_count++;
				block.clear();
				block = line;
				if (this->parse_block_name(line, block_type,
										   block_name, extends_type) == false)
				{
					fprintf(stderr, "Invalid proto file line: %s\n",
							line.c_str());
					fprintf(stderr, "Failed to parse block name or value.\n");
					return false;
				}
			}
		}
		else
			flag_append = true;

		// not else, because { }; can in the same line
		if ((state & PARSER_ST_BLOCK_MASK) == PARSER_ST_INSIDE_BLOCK)
		{
			if (flag_append == true)
				block.append(line);

			if (this->check_block_begin(line) == true)
				++stack_count;

			if (this->check_block_end(line) == true)
			{
				--stack_count;
				if (stack_count == 0)
				{
					state = PARSER_ST_OUTSIDE_BLOCK;
					//state &= PARSER_ST_OUTSIDE_BLOCK_MASK;
					Descriptor desc;
					if (block_type == "service")
					{
						if (this->is_thrift)
							succ = this->parse_service_thrift(info.file_name_prefix,
															  block, desc, info);
						else
							succ = this->parse_service_pb(block, desc);

						if (!succ)
							return false;

						desc.block_type = block_type;
						desc.block_name = block_name;
						desc.extends_type = extends_type;
						if (desc.extends_type != "" && this->is_thrift)
						{
							Descriptor *extended_desc = NULL;
							parse_thrift_type_name(desc.extends_type,
												   thrift_type_prefix,
												   thrift_type_name);

							if (thrift_type_prefix == "")
								extended_desc = search_cur_file_descriptor(info,
													"service", thrift_type_name);
							else
								extended_desc = search_include_file_descriptor(info,
													thrift_type_prefix+".thrift",
													"service", thrift_type_name);
							if (!extended_desc)
							{
								fprintf(stderr,"service %s extends type %s not found\n",
										desc.block_name.c_str(),
										desc.extends_type.c_str());
							}
							else
							{
								desc.rpcs.insert(desc.rpcs.begin(),
												 extended_desc->rpcs.begin(),
												 extended_desc->rpcs.end());
							}
						}

						info.desc_list.emplace_back(std::move(desc));
					}
					else if (block_type == "struct" && this->is_thrift)
					{
						succ = this->parse_struct_thrift(info.file_name_prefix,
														 block, desc, info);
						if (!succ)
							return false;

						desc.block_type = block_type;
						desc.block_name = block_name;
						info.desc_list.emplace_back(std::move(desc));

					}
					else if (block_type == "exception" && this->is_thrift)
					{
						succ = this->parse_struct_thrift(info.file_name_prefix,
														 block, desc, info);
						if (!succ)
							return false;

						desc.block_type = "struct";
						desc.block_name = block_name;
						info.desc_list.emplace_back(std::move(desc));
					}
					else if (block_type == "enum" && this->is_thrift)
					{
						succ = this->parse_enum_thrift(block, desc);
						if (!succ)
							return false;

						SGenUtil::get_enum_set()->insert(info.file_name_prefix + block_name);
						desc.block_type = block_type;
						desc.block_name = block_name;
						info.desc_list.emplace_back(std::move(desc));
					}
				}
			}
		}
	}

	build_typedef_mapping(info);
	fclose(in);
	fprintf(stdout, "finish parsing proto file: [%s]\n", proto_file.c_str());
	return true;
}

std::string Parser::find_typedef_mapping_type(std::string& type_name,
											  size_t& cur, idl_info& info)
{
	size_t st = cur;
	cur = SGenUtil::find_next_nonspace(type_name,cur);

	for (; cur < type_name.size(); cur++)
	{
		if (type_name[cur] == ',' || type_name[cur] == '>' ||
			type_name[cur] == '<')
			break;
	}

	auto idl_type = SGenUtil::strip(type_name.substr(st, cur - st));

	if (info.typedef_mapping.find(idl_type) != info.typedef_mapping.end())
		return info.typedef_mapping[idl_type];

	if (idl_type == "bool" ||
		idl_type == "int8_t" ||
		idl_type == "int16_t" ||
		idl_type == "int32_t" ||
		idl_type == "int64_t" ||
		idl_type == "uint64_t" ||
		idl_type == "double" ||
		idl_type == "std::string")
	{
		return idl_type;
	}
	else if (idl_type == "std::map" && cur < type_name.size() &&
			 type_name[cur] == '<')
	{
		auto key_type = find_typedef_mapping_type(type_name, ++cur, info);
		auto val_type = find_typedef_mapping_type(type_name, ++cur, info);
		return "std::map<" + key_type + ", " + val_type + ">";
	}
	else if (idl_type == "std::set" && cur < type_name.size() &&
			 type_name[cur] == '<')
	{
		auto val_type = find_typedef_mapping_type(type_name, ++cur, info);
		return "std::set<" + val_type + ">";
	}
	else if (idl_type == "std::vector" && cur < type_name.size() &&
			 type_name[cur] == '<')
	{
		auto val_type = find_typedef_mapping_type(type_name, ++cur, info);
		return "std::vector<" + val_type + ">";
	}

	size_t pos = idl_type.find("::",0);
	std::string real_type_name;
	std::string type_namespace;
	if (pos == std::string::npos)
		real_type_name = idl_type;
	else
	{
		real_type_name = idl_type.substr(pos+2);
		type_namespace = idl_type.substr(0,pos);
	}

	for (auto& include : info.include_list)
	{
		if ( (type_namespace != "" && include.package_name.size() > 0 &&
			  include.package_name[0] == type_namespace) ||
			(type_namespace == "" && include.package_name.size() == 0) )
		{
			for (auto& t : include.typedef_list)
			{
				if (real_type_name  == t.new_type_name)
				{
					size_t offset = 0;
					include.typedef_mapping[real_type_name] =
						find_typedef_mapping_type(t.old_type_name, offset, include);

					return include.typedef_mapping[real_type_name];
				}
			}
		}
	}

	for (auto& t : info.typedef_list)
	{
		if (real_type_name  == t.new_type_name)
		{
			size_t offset = 0;
			info.typedef_mapping[real_type_name] =
				find_typedef_mapping_type(t.old_type_name, offset, info);
			return info.typedef_mapping[real_type_name];
		}
	}

	return idl_type;
}


void Parser::build_typedef_mapping(idl_info& info)
{
	for (auto &include:info.include_list)
	{
		for (auto &t:include.typedef_list)
		{
			if (include.typedef_mapping.find(t.new_type_name) != include.typedef_mapping.end())
				continue;

			size_t cur = 0;
			include.typedef_mapping[t.new_type_name] =
				find_typedef_mapping_type(t.old_type_name, cur, include);
		}
	}

	for (auto &t:info.typedef_list)
	{
		if (info.typedef_mapping.find(t.new_type_name) != info.typedef_mapping.end())
			continue;

		size_t cur = 0;
		info.typedef_mapping[t.new_type_name] =
			find_typedef_mapping_type(t.old_type_name, cur, info);
	}
}


// check / * and cut the first available line
bool Parser::check_multi_comments_begin(std::string& line)
{
	size_t pos = line.find("/*");
	if (pos == std::string::npos)
		return false;

	line = line.substr(0, pos);
	return true;
}

// check * / and cut the rest available line
bool Parser::check_multi_comments_end(std::string& line)
{
	size_t pos = line.find("*/");
	if (pos == std::string::npos)
		return false;

	pos += 2;

	while (line[pos] == ' ')
		pos++;

	line = line.substr(pos, line.length() - 1 - pos);
	return true;
}

// [ret] 0: no rpc option; 1: rpc_option = false; 2: rpc_option = true;
int Parser::parse_pb_rpc_option(const std::string& line)
{
	size_t pos = line.find("option");
	if (pos == std::string::npos)
		return 0;
	pos = line.find("cc_generic_services", pos);
	if (pos == std::string::npos)
		return 0;
	pos = line.find("true", pos);
	if (pos == std::string::npos)
		return 1;
	return 2;
}

bool Parser::parse_dir_prefix(const std::string& file_name, char *dir_prefix)
{
	size_t pos = file_name.length() - 1;
	while (file_name[pos] != '/' && pos != 0)
		pos--;

	if (pos == 0)
		return false;

	snprintf(dir_prefix, pos + 2, "%s", file_name.c_str());
//	fprintf(stderr, "[%s]------------[%s]\n", file_name.c_str(), dir_prefix);
	return true;
}

bool Parser::parse_thrift_typedef(const std::string& line,
								  std::string& old_type_name,
								  std::string& new_type_name,
								  idl_info&info)
{
	std::vector<std::string> elems = SGenUtil::split_by_space(line);

	if (elems.size() >= 3 && elems[0] == "typedef")
	{
		size_t offset = 0;
		std::string idl_type;
		for (size_t i = 1; i < elems.size()-1; i++)
			idl_type.append(elems[i]);

		old_type_name = gen_param_var(idl_type,offset,info);
		new_type_name = elems[elems.size()-1];

		return true;
	}

	return false;
}

bool Parser::parse_include_file(const std::string& line, std::string& file_name)
{
	std::string include_prefix = (this->is_thrift ? "include" : "import");

	size_t pos = line.find(include_prefix);
	if (pos != 0)
		return false;

	auto st = line.find_first_of('\"');
	auto ed = line.find_last_of('\"');

	if (st == std::string::npos || ed == std::string::npos || st == ed)
		return false;

	file_name = line.substr(st + 1, ed - st - 1);
//	fprintf(stderr, "parse_include_file(%s,%s)\n", line.c_str(), file_name.c_str());
	return true;
}

bool Parser::parse_package_name(const std::string& line,
								std::vector<std::string>& package_name)
{
	std::string package_prefix = (this->is_thrift ? "namespace" : "package");

	size_t pos = line.find(package_prefix);
	if (pos != 0)
		return false;

	pos += package_prefix.length();
	while (pos < line.length() && isspace(line[pos]))
		pos++;

	if (this->is_thrift)
	{
		pos = line.find("cpp", pos);
		if (pos == std::string::npos)
			return false;

		pos += 3;
		while (pos < line.length() && isspace(line[pos]))
			pos++;
	}

	size_t begin;
	if (this->is_thrift)
	{
		begin = line.find_last_of('/');
		if (begin == std::string::npos)
			begin = pos;
		else
			begin++;
	} else {
		begin = pos;
	}

	while (pos < line.length() && !isspace(line[pos]) &&
		   line[pos] != ';')
	{
		pos++;
	}

	std::string names = line.substr(begin, pos - begin);

	pos = names.find('.');
	while (pos != (size_t)-1)
	{
		package_name.push_back(names.substr(0, pos));
		names = names.substr(pos + 1, names.length() - pos);
		pos = names.find('.');
	}
	package_name.push_back(names.substr(0, pos));

	return true;
}

static std::string gen_param_var(const std::string& type_name, size_t& cur,
								 idl_info& info)
{
	size_t st = cur;
	cur = SGenUtil::find_next_nonspace(type_name,cur);

	for (; cur < type_name.size(); cur++)
	{
		if (type_name[cur] == ',' || type_name[cur] == '>' ||
			type_name[cur] == '<')
			break;
	}

	auto idl_type = SGenUtil::strip(type_name.substr(st, cur - st));

	if (idl_type == "bool")
		return "bool";
	else if (idl_type == "i8" || idl_type == "byte")
		return "int8_t";
	else if (idl_type == "i16")
		return "int16_t";
	else if (idl_type == "i32")
		return "int32_t";
	else if (idl_type == "i64")
		return "int64_t";
	else if (idl_type == "u64")
		return "uint64_t";
	else if (idl_type == "double")
		return "double";
	else if (idl_type == "string" || idl_type == "binary")
		return "std::string";
	else if (idl_type == "map" && cur < type_name.size() &&
			 type_name[cur] == '<')
	{
		auto key_type = gen_param_var(type_name, ++cur, info);
		auto val_type = gen_param_var(type_name, ++cur, info);
		return "std::map<" + key_type + ", " + val_type + ">";
	}
	else if (idl_type == "set" && cur < type_name.size() &&
			 type_name[cur] == '<')
	{
		auto val_type = gen_param_var(type_name, ++cur, info);
		return "std::set<" + val_type + ">";
	}
	else if (idl_type == "list" && cur < type_name.size() &&
			 type_name[cur] == '<')
	{
		auto val_type = gen_param_var(type_name, ++cur, info);
		return "std::vector<" + val_type + ">";
	}

	return type_prefix_to_namespace(idl_type, info);
}

static void fill_rpc_param_type(const std::string& file_name_prefix,
								const std::string idl_type,
								rpc_param& param, idl_info& info)
{
	if (idl_type == "bool")
	{
		param.data_type = srpc::TDT_BOOL;
		param.type_name = "bool";
	}
	else if (idl_type == "i8" || idl_type == "byte")
	{
		param.data_type = srpc::TDT_I08;
		param.type_name = "int8_t";
	}
	else if (idl_type == "i16")
	{
		param.data_type = srpc::TDT_I16;
		param.type_name = "int16_t";
	}
	else if (idl_type == "i32")
	{
		param.data_type = srpc::TDT_I32;
		param.type_name = "int32_t";
	}
	else if (idl_type == "i64")
	{
		param.data_type = srpc::TDT_I64;
		param.type_name = "int64_t";
	}
	else if (idl_type == "u64")
	{
		param.data_type = srpc::TDT_U64;
		param.type_name = "uint64_t";
	}
	else if (idl_type == "double")
	{
		param.data_type = srpc::TDT_DOUBLE;
		param.type_name = "double";
	}
	else if (idl_type == "string" || idl_type == "binary")
	{
		param.data_type = srpc::TDT_STRING;
		param.type_name = "std::string";
	}
	else if (SGenUtil::start_with(idl_type, "list"))
	{
		size_t cur = 0;
		param.data_type = srpc::TDT_LIST;
		param.type_name = gen_param_var(idl_type, cur, info);
	}
	else if (SGenUtil::start_with(idl_type, "map"))
	{
		size_t cur = 0;
		param.data_type = srpc::TDT_MAP;
		param.type_name = gen_param_var(idl_type, cur, info);
	}
	else if (SGenUtil::start_with(idl_type, "set"))
	{
		size_t cur = 0;
		param.data_type = srpc::TDT_SET;
		param.type_name = gen_param_var(idl_type, cur, info);
	}
	else
	{
		auto *enum_set = SGenUtil::get_enum_set();
		if (enum_set->count(idl_type) > 0 ||
			enum_set->count(file_name_prefix + idl_type) > 0)
		{
			//enum
			param.data_type = srpc::TDT_I32;
			param.type_name = "int32_t";
		}
		else
		{
			//struct
			param.data_type = srpc::TDT_STRUCT;
			param.type_name = type_prefix_to_namespace(idl_type,info);
		}
	}
}

std::vector<std::string> Parser::split_thrift_rpc(const std::string& str)
{
	std::vector<std::string> res;
	std::string::const_iterator start = str.begin();
	std::string::const_iterator parameter_end = str.end();
	std::string::const_iterator throws_end = str.end();
	while (1)
	{
		parameter_end = find(start,str.end(), ')');
		if (parameter_end == str.end()) 
		{
			res.emplace_back(start,parameter_end);
			break;
		}

		std::string::const_iterator offset = find_if(parameter_end + 1, str.end(),
					[](char c){return !std::isspace(c);});
		if (offset == str.end())
		{
			res.emplace_back(start,parameter_end);
			break;
		}

		if (str.compare(offset-str.begin(), 6, "throws") == 0)
		{
			throws_end = find(offset,str.end(), ')');
			res.emplace_back(start,throws_end);
			start = throws_end + 1;
		}
		else
		{
			res.emplace_back(start,parameter_end);
			start = parameter_end + 1;
		}
	}
	return res;
}

bool Parser::parse_rpc_param_thrift(const std::string& file_name_prefix,
									const std::string& str,
									std::vector<rpc_param>& params,
									idl_info& info)
{
	size_t left_b = 0;
	rpc_param param;
	std::string idl_type;
	if (left_b + 1 < str.size())
	{
		auto bb = SGenUtil::split_filter_empty(str.substr(left_b + 1), ',');
		for (const auto& ele : bb)
		{
			auto filedparam = SGenUtil::split_filter_empty(ele, ':');
			if (filedparam.size() != 2)
			  continue;

			auto typevar = SGenUtil::split_filter_empty(filedparam[1], ' ');
			if (typevar.size() != 2)
			  continue;

			param.var_name = typevar[1];
			param.required_state = srpc::THRIFT_STRUCT_FIELD_REQUIRED;
			param.field_id = atoi(SGenUtil::strip(filedparam[0]).c_str());
			idl_type = SGenUtil::strip(typevar[0]);
			fill_rpc_param_type(file_name_prefix, idl_type, param, info);
			params.push_back(param);
		}
	}
	return true;
}

bool Parser::parse_service_thrift(const std::string& file_name_prefix,
								  const std::string& block,
								  Descriptor& desc,
								  idl_info& info)
{
	rpc_descriptor rpc_desc;
	auto st = block.find_first_of('{');
	auto ed = block.find_last_of('}');
	if (st == std::string::npos || ed == std::string::npos || st == ed)
		return false;

	std::string valid_block = block.substr(st + 1, ed - st - 1);
	auto arr = split_thrift_rpc(valid_block);

	for (const auto& ele : arr)
	{
		auto line = SGenUtil::strip(ele);
		size_t i = 0;
		for (; i < line.size(); i++)
		{
			if (line[i] != ';' && line[i] != ',' && !isspace(line[i]))
				break;
		}

		if (i == line.size())
			continue;

		line = line.substr(i);

		if (line.empty())
			continue;

		auto left_b = line.find('(');
		if (left_b == std::string::npos)
			continue;

		auto aa = SGenUtil::split_filter_empty(line.substr(0, left_b), ' ');
		if (aa.size() != 2)
			continue;

		rpc_desc.method_name = SGenUtil::strip(aa[1]);
		if (rpc_desc.method_name.empty())
			continue;

		rpc_desc.request_name = rpc_desc.method_name + "Request";
		rpc_desc.response_name = rpc_desc.method_name + "Response";

		auto idl_type = SGenUtil::strip(aa[0]);
		rpc_param param;

		param.var_name = "result";
		param.required_state = srpc::THRIFT_STRUCT_FIELD_REQUIRED;
		param.field_id = 0;
		if (idl_type != "void")
		{
			fill_rpc_param_type(file_name_prefix, idl_type, param, info);
			rpc_desc.resp_params.push_back(param);
		}

		auto right_b = line.find(')',left_b);
		if (right_b == std::string::npos)
		{
			parse_rpc_param_thrift(file_name_prefix, line.substr(left_b),
								   rpc_desc.req_params,info);
		}
		else
		{
			parse_rpc_param_thrift(file_name_prefix,
								   line.substr(left_b, right_b - left_b),
								   rpc_desc.req_params,
								   info);

			auto throws_start = line.find("throws", right_b);
			if (throws_start != std::string::npos)
			{
				left_b = line.find('(', throws_start + 6);
				parse_rpc_param_thrift(file_name_prefix, line.substr(left_b),
									   rpc_desc.resp_params, info);
			}
		}

		fprintf(stdout, "Successfully parse method:%s req:%s resp:%s\n",
				rpc_desc.method_name.c_str(),
				rpc_desc.request_name.c_str(),
				rpc_desc.response_name.c_str());
		desc.rpcs.emplace_back(std::move(rpc_desc));
	}

	return true;
}

void Parser::check_single_comments(std::string& line)
{
	size_t pos = line.find("#");
	if (pos == std::string::npos)
		pos = line.find("//");

	if (pos != std::string::npos)
		line.resize(pos);
}

bool Parser::parse_enum_thrift(const std::string& block, Descriptor& desc)
{
	rpc_param param;
	auto st = block.find_first_of('{');
	auto ed = block.find_last_of('}');
	if (st == std::string::npos || ed == std::string::npos || st == ed)
		return false;

	std::string valid_block = block.substr(st + 1, ed - st - 1);
	for (size_t i = 0; i < valid_block.size(); i++)
		if (valid_block[i] == '\n' || valid_block[i] == '\r' || valid_block[i] == ',')
			valid_block[i] = ';';

	auto arr = SGenUtil::split_filter_empty(valid_block, ';');

	for (const auto& ele : arr)
	{
		auto line = SGenUtil::strip(ele);
		if (line.empty())
			continue;

		if (line.back() == ';' || line.back() == ',')
			line.resize(line.size() - 1);

		desc.enum_lines.push_back(line);
	}

	return true;
}

bool Parser::parse_struct_thrift(const std::string& file_name_prefix,
								 const std::string& block,
								 Descriptor& desc, idl_info& info)
{
	auto st = block.find_first_of('{');
	auto ed = block.find_last_of('}');
	if (st == std::string::npos || ed == std::string::npos || st == ed)
		return false;

	std::string valid_block = block.substr(st + 1, ed - st - 1);
	int deep = 0;
	for (size_t i = 0; i < valid_block.size(); i++)
	{
		if (valid_block[i] == '\n' || valid_block[i] == '\r')
			valid_block[i] = ';';
		else if (valid_block[i] == ',' && deep == 0)
			valid_block[i] = ';';
		else if (valid_block[i] == '<')
			deep++;
		else if (valid_block[i] == '>')
			deep--;
	}

	auto arr = SGenUtil::split_filter_empty(valid_block, ';');

	for (const auto& ele : arr)
	{
		auto line = SGenUtil::strip(ele);
		if (line.empty())
			continue;

		auto aabb = SGenUtil::split_filter_empty(line, ':');
		if (aabb.size() != 2)
			continue;

		auto aa = SGenUtil::strip(aabb[0]);
		if (aa.empty())
			continue;

		rpc_param param;
		param.field_id = atoi(aa.c_str());

		auto bb = SGenUtil::strip(aabb[1]);
		auto bbcc = SGenUtil::split_filter_empty(bb, '=');
		if (bbcc.size() == 2)
		{
			bb = SGenUtil::strip(bbcc[0]);
			param.default_value = SGenUtil::strip(bbcc[1]);
		}

		auto idx1 = std::string::npos;//bb.find_first_of(' ');
		auto idx2 = std::string::npos;//bb.find_last_of(' ');

		for (size_t i = 0; i < bb.size(); i++)
		{
			if (isspace(bb[i]))
			{
				idx1 = i;
				break;
			}
		}

		for (size_t i = 0; i < bb.size(); i++)
		{
			if (isspace(bb[bb.size() - i - 1]))
			{
				idx2 = bb.size() - i - 1;
				break;
			}
		}

		if (idx1 == std::string::npos || idx2 == std::string::npos)
			continue;

		std::string b1 = SGenUtil::strip(bb.substr(0, idx1));
		std::string b2, b3;

		if (idx1 == idx2 || (b1 != "required" && b1 != "optional"))
		{
			param.required_state = srpc::THRIFT_STRUCT_FIELD_DEFAULT;
			b1 = "default";
			b2 = SGenUtil::strip(bb.substr(0, idx2));
			b3 = SGenUtil::strip(bb.substr(idx2 + 1));
		}
		else
		{
			param.required_state = (b1 == "required") ?
								   srpc::THRIFT_STRUCT_FIELD_REQUIRED :
								   srpc::THRIFT_STRUCT_FIELD_OPTIONAL;
			b2 = SGenUtil::strip(bb.substr(idx1 + 1, idx2 - idx1 - 1));
			b3 = SGenUtil::strip(bb.substr(idx2 + 1));
		}

		if (b1.empty() || b2.empty() || b3.empty())
			continue;

		param.var_name = b3;

		fill_rpc_param_type(file_name_prefix, b2, param, info);
		fprintf(stdout, "Successfully parse struct param: %s %s %s\n",
				param.type_name.c_str(), param.var_name.c_str(),
			    param.default_value.c_str());
		desc.st.params.push_back(param);
	}

	return true;
}

bool Parser::parse_service_pb(const std::string& block, Descriptor& desc)
{
	size_t pos = block.find("{");
	if (pos == std::string::npos)
		return false;

	while (pos < block.length())
	{
		rpc_descriptor rpc_desc;
		pos = block.find("rpc", pos);
		if (pos == std::string::npos)
		{
			if (desc.rpcs.size() == 0)
			{
				fprintf(stderr, "no \"rpc\" in service block [%s]\n",
						block.c_str());
				return false;
			} else {
				return true;
			}
		}
		pos += strlen("rpc");
		while (block[pos] == ' ' && pos < block.length())
			pos++;
		if (pos == block.length())
			return false;

		size_t method_name_pos = block.find("(", pos);
		if (method_name_pos == std::string::npos)
		{
			fprintf(stderr, "no method_name in service block [%s]\n",
					block.c_str());
			return false;
		}

		rpc_desc.method_name = std::string(&block[pos], &block[method_name_pos]);
		rpc_desc.method_name.erase(rpc_desc.method_name.find_last_not_of(" ") + 1);

		pos = method_name_pos;
		while (block[pos] == ' ' && pos < block.length())
			pos++;
		if (pos == block.length())
			return false;

		size_t request_name_pos = block.find(")", pos + 1);
		if (request_name_pos == std::string::npos)
		{
			fprintf(stderr, "no request_name in service block [%s]\n",
					block.c_str());
			return false;
		}
		rpc_desc.request_name = std::string(&block[pos + 1],
											&block[request_name_pos]);

		pos = block.find("returns", pos + 1);
		if (pos == std::string::npos)
		{
			fprintf(stderr, "no \"returns\" in service block [%s]\n",
					block.c_str());
			return false;
		}

		while (block[pos] == ' ' && pos < block.length())
			pos++;
		if (pos == block.length())
			return true;

		size_t response_name_pos = block.find("(", pos + 1);
		size_t response_name_end = block.find(")", pos + 1);
		if (response_name_pos == std::string::npos ||
			response_name_end == std::string::npos)
		{
			fprintf(stderr, "no response_name in service block [%s]\n",
					block.c_str());
			return false;
		}

		rpc_desc.response_name = std::string(&block[response_name_pos + 1],
											 &block[response_name_end]);
		fprintf(stdout, "Successfully parse method:%s req:%s resp:%s\n",
				rpc_desc.method_name.c_str(),
				rpc_desc.request_name.c_str(),
				rpc_desc.response_name.c_str());
		desc.rpcs.emplace_back(std::move(rpc_desc));
		pos = response_name_end;
	}
	return true;
}

bool Parser::parse_block_name(const std::string& line,
							  std::string& block_name,
							  std::string& block_name_value,
							  std::string& extends_type)
{
	size_t pos = line.find("{");
	if (pos == std::string::npos)
	{
		fprintf(stderr, "failed to parse block name in %s\n",line.c_str());
		return false;
	}

	std::vector<std::string> elems = SGenUtil::split_by_space(line.substr(0,pos));
	if (elems.size() == 2)
	{
		block_name = elems[0];
		block_name_value = elems[1];
		extends_type = "";
	}
	else if (this->is_thrift && elems.size() == 4 && elems[0] == "service" &&
			 elems[2] == "extends")
	{
		block_name = elems[0];
		block_name_value = elems[1];
		extends_type = elems[3];
	}
	else
	{
		fprintf(stderr, "failed to parse block name in %s\n", line.c_str());
		return false;
	}
	fprintf(stdout, "Successfully parse service block [%s] : %s\n",
			block_name.c_str(), block_name_value.c_str());
	return true;
}

/*
bool Parser::parse_block_name(const std::string& line,
							  std::string& block_name,
							  std::string& block_name_value)
{
	size_t pos = line.find_first_of(" ", 0);
	if (pos == std::string::npos)
	{
		fprintf(stderr, "failed to parse rpc name in %s\n", line.c_str());
		return false;
	}

	block_name = std::string(&line[0], &line[pos]);

	size_t value_pos = line.find_first_of(" ", pos + 1);
	if (value_pos != std::string::npos)
	{
		block_name_value = std::string(&line[pos + 1], &line[value_pos]);
	} else {
		size_t end = line.find("{");
		if (end == std::string::npos)
			return false;

		end--;
		while (line[end] == '\n' || line[end] == '\r' || line[end] == ' ')
			end --;

//		block_name_value = std::string(&line[pos + 1], &line[line.length() - 1]);
		block_name_value = std::string(&line[pos + 1], &line[end + 1]);
	}

	fprintf(stdout, "Successfully parse service block [%s] : %s\n",
			block_name.c_str(), block_name_value.c_str());
	return true;
}
*/

bool Parser::check_block_begin(FILE *in, std::string& line)
{
	if (line.find(";") != std::string::npos)
		return false;

	if (line.find("{") != std::string::npos)
		return true;

	char line_buffer[LINE_LENGTH_MAX];

	if (fgets(line_buffer, LINE_LENGTH_MAX, in))
	{
		std::string next = SGenUtil::strip(line_buffer);
		if (next.find("{") != std::string::npos)
		{
			line.append(next.c_str());
			return true;
		}
	}
	return false;
}

bool Parser::check_block_begin(const std::string& line)
{
	if (line.find("{") == std::string::npos)
		return false;
	return true;
}

bool Parser::check_block_end(const std::string& line)
{
	if (line.find("}") == std::string::npos
		&& line.find("};") == std::string::npos)
		return false;
	return true;
}

void parse_thrift_type_name(const std::string& type_name,
									std::string& type_prefix,
									std::string& real_type_name)
{
	size_t pos = type_name.find('.',0);
	if (pos == std::string::npos)
	{
		type_prefix = "";
		real_type_name = SGenUtil::strip(type_name);
	}
	else
	{
		type_prefix = SGenUtil::strip(type_name.substr(0,pos));
		real_type_name = SGenUtil::strip(type_name.substr(pos+1));
	}
}

std::string type_prefix_to_namespace(const std::string& type_name,
									 idl_info& info)
{
	std::string prefix;
	std::string real_type;
	parse_thrift_type_name(type_name, prefix, real_type);
	if (prefix == "")
		return type_name;

	idl_info *include = search_include_file(info, prefix + ".thrift");
	if (include == NULL)
	{
		fprintf(stderr,"cannot find type %s\n",type_name.c_str());
		return type_name;
	}

	if (include->package_name.size() > 0)
		return include->package_name[0]+"::"+real_type;

	return "::"+real_type;
}

Descriptor *search_cur_file_descriptor(idl_info& info,
									   const std::string& block_type,
									   const std::string& block_name)
{
	for (auto &desc : info.desc_list)
	{
		if (desc.block_type == block_type && desc.block_name == block_name)
			return &desc;
	}
	return NULL;
}

Descriptor *search_include_file_descriptor(idl_info& info,
										   const std::string include_file_name,
										   const std::string& block_type,
										   const std::string& block_name)
{
	for (auto &include : info.include_list)
	{
		if (include.file_name == include_file_name)
			return search_cur_file_descriptor(include, block_type, block_name);
	}
	return NULL;
}

idl_info *search_include_file(idl_info& info, const std::string file_name)
{
	for (auto &include : info.include_list)
	{
		if (include.file_name == file_name)
			return &include;
	}
	return NULL;
}

idl_info *search_namespace(idl_info& info, const std::string name_space)
{
	if (name_space == "")
		return &info;

	for (auto &include : info.include_list)
	{
		if (include.package_name.size() > 0 &&
			include.package_name[0] == name_space)
			return &include;
	}
	return NULL;
}

int Parser::find_valid(const std::string& line)
{
/*
	char *p = line.c_str();
	char *q = p;
	while (q != p + line.length())
	{
		if(*q != ' ' && *q != '\t')
			return pos;
	}
*/
	return 0;
}

