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
#include "parser.h"
#include "thrift/rpc_thrift_enum.h"

#define LINE_MAX 2048

bool Parser::parse(const std::string& proto_file, idl_info& info)
{
	std::string dir_prefix;
	auto pos = proto_file.find_last_of('/');
	if (pos == std::string::npos)
		info.file_name = proto_file;
	else
	{
		info.file_name = proto_file.substr(pos + 1);
		dir_prefix = proto_file.substr(0, pos + 1);
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
		fprintf(stderr, "[Parser] proto file: [%s] not exists.\n", proto_file.c_str());
		return false;
	}

	fprintf(stdout, "proto file: [%s]\n", proto_file.c_str());

	char line_buffer[LINE_MAX];
	std::string file_path;
	std::string block;
	int state = PARSER_ST_OUTSIDE_BLOCK;
	bool flag_append;
	int stack_count = 0;
	std::string block_name;
	std::string block_type;
	bool succ;

	while (fgets(line_buffer, LINE_MAX, in))
	{
		std::string line = SGenUtil::lstrip(line_buffer);

		if (line.empty())
			continue;

		this->check_single_comments(line);
		if (line.empty())
			continue;

		if ((state & PARSER_ST_COMMENT_MASK) == PARSER_ST_INSIDE_COMMENT)
		{
			if (this->check_multi_comments_end(line))
				state -= PARSER_ST_INSIDE_COMMENT;
			//	state |= PARSER_ST_OUTSIDE_COMMENT_MASK;
			else
				continue;

		} else if (this->check_multi_comments_begin(line)) {
			state = (state & PARSER_ST_OUTSIDE_COMMENT_MASK) + PARSER_ST_INSIDE_COMMENT;
		}

		if (this->is_thrift == false)
		{
			int rpc_option = this->parse_pb_rpc_option(line);
			if (rpc_option == 1)
				continue;
			else if (rpc_option == 2)
			{
				fprintf(stderr, "[Parser ERROR] %s must not set "
						"\"option cc_generic_services = true\" for srpc.\n", proto_file.c_str());
				return false;
			}
		}

		if (this->parse_package_name(line, info.package_name) == true)
			continue;

		if (this->parse_include_file(line, file_path) == true)
		{
			if (file_path[0] != '/')
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
										   block_name) == false)
				{
					fprintf(stderr, "Invalid proto file line: %s\n", line.c_str());
					fprintf(stderr, "Failed to parse block name or value.\n");
					return false;
				}
			}
		} else {
			flag_append = true;
		}

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
					if (block_type == "service")
					{
						Descriptor desc;
						if (this->is_thrift)
							succ = this->parse_service_thrift(info.file_name_prefix, block, desc);
						else
							succ = this->parse_service_pb(block, desc);

						if (!succ)
							return false;

						desc.block_type = block_type;
						desc.block_name = block_name;
						info.desc_list.emplace_back(std::move(desc));
					}
					else if (block_type == "struct" && this->is_thrift)
					{
						Descriptor desc;
						succ = this->parse_struct_thrift(info.file_name_prefix, block, desc);
						if (!succ)
							return false;

						desc.block_type = block_type;
						desc.block_name = block_name;
						info.desc_list.emplace_back(std::move(desc));

					}
					else if (block_type == "enum" && this->is_thrift)
					{
						Descriptor desc;
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

	fclose(in);
	return true;
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
//	fprintf(stderr, "parse_include_file() %s--------%s\n", line.c_str(), file_name.c_str());
	return true;

/*
	pos += include_prefix.length();

	while (line[pos] == ' ')
		pos++;

	size_t begin = pos;

	//TODO: special logic for thrift
	while (pos < line.length() && !isspace(line[pos])
			&& line[pos] != ';')
		pos++;

	if (line[begin] != '\"' || line[line.length() - 2] != ';'
		|| line[line.length() - 3] != '\"')
		return false;

	file_name = line.substr(begin + 1, line.length() - 4 - begin);
	return true;*/
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

	while (pos < line.length() && !isspace(line[pos])
			&& line[pos] != ';')
		pos++;

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

static std::string gen_param_var(const std::string& type_name, size_t& cur)
{
	size_t st = cur;

	for (; cur < type_name.size(); cur++)
	{
		if (type_name[cur] == ',' || type_name[cur] == '>' || type_name[cur] == '<')
			break;
	}

	auto idl_type = SGenUtil::strip(type_name.substr(st, cur - st));

	if (type_name[cur] == ',' || type_name[cur] == '>')
		cur++;

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
	else if (idl_type == "map" && cur < type_name.size() && type_name[cur] == '<')
	{
		auto key_type = gen_param_var(type_name, ++cur);
		auto val_type = gen_param_var(type_name, ++cur);
		return "std::map<" + key_type + ", " + val_type + ">";
	}
	else if (idl_type == "set" && cur < type_name.size() && type_name[cur] == '<')
	{
		auto val_type = gen_param_var(type_name, ++cur);
		return "std::set<" + val_type + ">";
	}
	else if (idl_type == "list" && cur < type_name.size() && type_name[cur] == '<')
	{
		auto val_type = gen_param_var(type_name, ++cur);
		return "std::vector<" + val_type + ">";
	}

	return idl_type;
}

static void fill_rpc_param_type(const std::string& file_name_prefix,
								const std::string& idl_type, rpc_param& param)
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
		param.type_name = gen_param_var(idl_type, cur);
	}
	else if (SGenUtil::start_with(idl_type, "map"))
	{
		size_t cur = 0;
		param.data_type = srpc::TDT_MAP;
		param.type_name = gen_param_var(idl_type, cur);
	}
	else if (SGenUtil::start_with(idl_type, "set"))
	{
		size_t cur = 0;
		param.data_type = srpc::TDT_SET;
		param.type_name = gen_param_var(idl_type, cur);
	}
	else
	{
		auto *enum_set = SGenUtil::get_enum_set();
		if (enum_set->count(idl_type) > 0 || enum_set->count(file_name_prefix + idl_type) > 0)
		{
			//enum
			param.data_type = srpc::TDT_I32;
			param.type_name = "int32_t";
		}
		else
		{
			//struct
			param.data_type = srpc::TDT_STRUCT;
			param.type_name = idl_type;
		}
	}
}

bool Parser::parse_service_thrift(const std::string& file_name_prefix, const std::string& block, Descriptor& desc)
{
	rpc_descriptor rpc_desc;
	auto st = block.find_first_of('{');
	auto ed = block.find_last_of('}');
	if (st == std::string::npos || ed == std::string::npos || st == ed)
		return false;

	std::string valid_block = block.substr(st + 1, ed - st - 1);
	auto arr = SGenUtil::split_filter_empty(valid_block, ')');

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
			fill_rpc_param_type(file_name_prefix, idl_type, param);
			rpc_desc.resp_params.push_back(param);
		}

		if (left_b + 1 < line.size())
		{
			auto bb = SGenUtil::split_filter_empty(line.substr(left_b + 1), ',');
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
				fill_rpc_param_type(file_name_prefix, idl_type, param);
				rpc_desc.req_params.push_back(param);
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
								 const std::string& block, Descriptor& desc)
{
	rpc_param param;
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

		param.field_id = atoi(aa.c_str());

		auto bb = SGenUtil::strip(aabb[1]);
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
			param.required_state = (b1 == "required") ? srpc::THRIFT_STRUCT_FIELD_REQUIRED
													  : srpc::THRIFT_STRUCT_FIELD_OPTIONAL;
			b2 = SGenUtil::strip(bb.substr(idx1 + 1, idx2 - idx1 - 1));
			b3 = SGenUtil::strip(bb.substr(idx2 + 1));
		}

		if (b1.empty() || b2.empty() || b3.empty())
			continue;

		param.var_name = b3;

		fill_rpc_param_type(file_name_prefix, b2, param);
		fprintf(stdout, "Successfully parse struct param: %s %s\n",
				param.type_name.c_str(), param.var_name.c_str());
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
				fprintf(stderr, "no \"rpc\" in service block [%s]\n", block.c_str());
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
			fprintf(stderr, "no method_name in service block [%s]\n", block.c_str());
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
			fprintf(stderr, "no request_name in service block [%s]\n", block.c_str());
			return false;
		}
		rpc_desc.request_name = std::string(&block[pos + 1], &block[request_name_pos]);

		pos = block.find("returns", pos + 1);
		if (pos == std::string::npos)
		{
			fprintf(stderr, "no \"returns\" in service block [%s]\n", block.c_str());
			return false;
		}

		while (block[pos] == ' ' && pos < block.length())
			pos++;
		if (pos == block.length())
			return true;

		size_t response_name_pos = block.find("(", pos + 1);
		size_t response_name_end = block.find(")", pos + 1);
		if (response_name_pos == std::string::npos
			|| response_name_end == std::string::npos)
		{
			fprintf(stderr, "no response_name in service block [%s]\n", block.c_str());
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

bool Parser::check_block_begin(FILE *in, std::string& line)
{
	if (line.find(";") != std::string::npos)
		return false;

	if (line.find("{") != std::string::npos)
		return true;

	char line_buffer[LINE_MAX];

	if (fgets(line_buffer, LINE_MAX, in))
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

