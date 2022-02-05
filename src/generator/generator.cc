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

#include "generator.h"

bool Generator::init_file_names(const std::string& idl_file, const char *out_dir)
{
	if (idl_file.size() <= 5)
		return false;

	const char *p;
	if (this->is_thrift)
	{
		p = idl_file.c_str() + idl_file.size() - 7;
		if (strncmp(p, ".thrift", 7) != 0)
			return false;
	}
	else
	{
		p = idl_file.c_str() + idl_file.size() - 6;
		if (strncmp(p, ".proto", 6) != 0)
			return false;
	}

	p = idl_file.c_str() + idl_file.size() - 1;
	while (p > idl_file.c_str())
	{
		if (*p == '/')
			break;
		p--;
	}
	if (*p == '/')
		p++;

	std::string str = p;
	auto pos = str.find(".");
	if (pos == std::string::npos)
		return false;

	this->prefix = str.substr(0, pos);
	str = out_dir;
	if (str[str.length() - 1] != '/')
		str.append("/");

	this->out_dir = str;

	this->srpc_file = str;
	this->srpc_file.append(this->prefix);
	this->srpc_file.append(this->suffix);
	this->srpc_file.append("h");

	this->thrift_type_file = str;
	this->thrift_type_file.append(this->prefix);
	this->thrift_type_file.append(this->thrift_suffix);
	this->thrift_type_file.append("h");

	fprintf(stdout, "[Generator] generate srpc files: %s %s\n",
			this->srpc_file.c_str(),
			this->is_thrift ? this->thrift_type_file.c_str() : "");

	return true;
}

bool Generator::generate(const std::string& idl_file, struct GeneratorParams params)
{
	if (this->parser.parse(idl_file, this->info) == false)
	{
		fprintf(stderr, "[Generator Error] parse failed.\n");
		return false;
	}

	if (this->generate_header(this->info, params) == false)
	{
		fprintf(stderr, "[Generator Error] generate failed.\n");
		return false;
	}

	this->generate_skeleton(this->info.file_name);

	return true;
}

bool Generator::generate_header(idl_info& cur_info, struct GeneratorParams params)
{
	for (auto& sub_info : cur_info.include_list)
	{
		fprintf(stdout, "[Generator] auto generate include file [%s]\n",
				sub_info.absolute_file_path.c_str());
		if (!this->generate_header(sub_info, params))
			return false;
	}

	// for protobuf: if no [rpc], don`t need to generate xxx.srpc.h
	if (this->init_file_names(cur_info.absolute_file_path, params.out_dir) == false)
	{
		fprintf(stderr, "[Generator Error] init file name failed. %s %s\n",
				cur_info.absolute_file_path.c_str(), params.out_dir);
		return false;
	}

	// will generate skeleton file only once
	if (this->is_thrift)
	{
		//[prefix].thrift.h
		if (!this->generate_thrift_type_file(cur_info))
		{
			fprintf(stderr, "[Generator Error] generate thrift type file failed.\n");
			return false;
		}
	}

	for (const auto& desc : this->info.desc_list)
	{
		if (desc.block_type == "service")
		{
			//has_service = true;
			if (!this->generate_srpc_file(cur_info)) // [prefix].srpc.h
			{
				fprintf(stderr, "[Generator Error] generate srpc file failed.\n");
				return false;
			}

			break;
		}
	}

	fprintf(stdout, "[Generator Done]\n");
	return true;
}

void Generator::generate_skeleton(const std::string& idl_file)
{
	const char *p;
	if (this->is_thrift)
	{
		p = idl_file.c_str() + idl_file.size() - 7;
		if (strncmp(p, ".thrift", 7) != 0)
			return;
	}
	else
	{
		p = idl_file.c_str() + idl_file.size() - 6;
		if (strncmp(p, ".proto", 6) != 0)
			return;
	}

	p = idl_file.c_str() + idl_file.size() - 1;
	while (p > idl_file.c_str())
	{
		if (*p == '/')
			break;
		p--;
	}
	if (*p == '/')
		p++;

	std::string str = p;
	auto pos = str.find(".");
	if (pos == std::string::npos)
		return;

	std::string idl_file_name = str.substr(0, pos);

	this->server_cpp_file = this->out_dir;
	this->server_cpp_file.append("server");
	this->server_cpp_file.append(this->is_thrift ? ".thrift_skeleton." : ".pb_skeleton.");
	this->server_cpp_file.append("cc");

	this->client_cpp_file = this->out_dir;
	this->client_cpp_file.append("client");
	this->client_cpp_file.append(this->is_thrift ? ".thrift_skeleton." : ".pb_skeleton.");
	this->client_cpp_file.append("cc");

	// server.skeleton.cc
	this->generate_server_cpp_file(this->info, idl_file_name);
	// client.skeleton.cc
	this->generate_client_cpp_file(this->info, idl_file_name);

	fprintf(stdout, "[Generator] generate server files: %s, client files: %s\n",
			this->server_cpp_file.c_str(),
			this->client_cpp_file.c_str());
	return;
}

void Generator::thrift_replace_include(const idl_info& cur_info, std::vector<rpc_param>& params)
{
	for (auto& param : params)
	{
		if (param.data_type == srpc::TDT_LIST
			|| param.data_type == srpc::TDT_MAP
			|| param.data_type == srpc::TDT_SET)
		{
			for (const auto& desc : cur_info.desc_list)
			{
				if (desc.block_type == "enum")
					SGenUtil::replace(param.type_name, desc.block_name, "int32_t");
			}

			for (const auto sub_info : cur_info.include_list)
			{
				for (const auto& desc : sub_info.desc_list)
				{
					if (desc.block_type == "enum")
						SGenUtil::replace(param.type_name, sub_info.file_name_prefix + desc.block_name, "int32_t");
				}
			}
		}

		auto pos = param.type_name.find('.');
		if (pos != std::string::npos)
		{
			for (const auto sub_info : cur_info.include_list)
			{
				if (sub_info.package_name.empty())
					continue;

				//printf("????%s %s\n", sub_info.file_name_prefix.c_str(), sub_info.package_name.c_str());
				SGenUtil::replace(param.type_name, sub_info.file_name_prefix,
								  "::" + join_package_names(sub_info.package_name) + "::");
			}
		}
	}
}

bool Generator::generate_thrift_type_file(idl_info& cur_info)
{

	if (!this->printer.open(this->thrift_type_file))
	{
		fprintf(stderr, "[Generator Error] can't write to thirft_type_file: %s.\n",
				this->thrift_type_file.c_str());
		return false;
	}

	this->printer.print_thrift_include(cur_info);
	this->printer.print_thrift_typedef(cur_info);
	this->printer.print_thrift_struct_declaration(cur_info);

	for (auto& desc : cur_info.desc_list)
	{
		if (desc.block_type == "service")
		{
			this->printer.print_service_namespace(desc.block_name);
			for (auto& rpc : desc.rpcs)
			{
				this->thrift_replace_include(cur_info, rpc.req_params);
				this->printer.print_rpc_thrift_struct_class(rpc.request_name, rpc.req_params,cur_info);
				this->thrift_replace_include(cur_info, rpc.resp_params);
				this->printer.print_rpc_thrift_struct_class(rpc.response_name, rpc.resp_params,cur_info);
			}

			this->printer.print_service_namespace_end(desc.block_name);
		}
		else if (desc.block_type == "struct")
		{
			this->thrift_replace_include(cur_info, desc.st.params);
			this->printer.print_rpc_thrift_struct_class(desc.block_name, desc.st.params,cur_info);
		}
		else if (desc.block_type == "enum")
		{
			this->printer.print_thrift_include_enum(desc.block_name, desc.enum_lines);
		}
	}

	this->printer.print_end(cur_info.package_name);
	this->printer.close();
	return true;
}

bool Generator::generate_srpc_file(const idl_info& cur_info)
{
	if (!this->printer.open(this->srpc_file))
	{
		fprintf(stderr, "[Generator Error] can't write to srpc file: %s.\n",
				this->srpc_file.c_str());
		return false;
	}

	this->printer.print_srpc_include(this->prefix, cur_info.package_name);

	std::vector<std::string> rpc_list;
	std::string package;

	if (this->is_thrift)
	{
		rpc_list.push_back("SRPC");
		rpc_list.push_back("SRPCHttp");
		rpc_list.push_back("Thrift");
		rpc_list.push_back("ThriftHttp");
	}
	else
	{
		rpc_list.push_back("SRPC");
		rpc_list.push_back("SRPCHttp");
		rpc_list.push_back("BRPC");
		rpc_list.push_back("TRPC");
		rpc_list.push_back("TRPCHttp");
	}

	for (const std::string& p : cur_info.package_name)
	{
		package.append(p);
		package.push_back('.');
	}

	for (const auto& desc : cur_info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		this->printer.print_service_namespace(desc.block_name);
		this->printer.print_server_comment();

		if (this->is_thrift)
			this->printer.print_server_class_thrift(desc.block_name, desc.rpcs);
		else
			this->printer.print_server_class(desc.block_name, desc.rpcs);

		this->printer.print_client_comment();
		for (const auto& rpc : desc.rpcs)
		{
			this->printer.print_client_define_done(rpc.method_name,
												   rpc.response_name);
		}

		this->printer.print_empty_line();

		for (const auto& type : rpc_list)
			this->printer.print_client_class(type, desc.block_name, desc.rpcs);

		this->printer.print_implement_comments();

		this->printer.print_server_constructor(package + desc.block_name, desc.rpcs);
		if (this->is_thrift)
			this->printer.print_server_methods_thrift(desc.block_name, desc.rpcs);

		for (const auto& type : rpc_list)
		{
			this->printer.print_client_constructor(type, desc.block_name,
												   cur_info.package_name);
			this->printer.print_client_methods(type, desc.block_name, desc.rpcs,
											   cur_info.package_name);
			this->printer.print_client_create_task(type, desc.block_name,
												   desc.rpcs, cur_info.package_name);
		}

		this->printer.print_service_namespace_end(desc.block_name);
	}

	this->printer.print_end(cur_info.package_name);
	this->printer.close();

	return true;
}

void Generator::generate_server_cpp_file(const idl_info& cur_info, const std::string& idl_file_name)
{
	this->printer.open(this->server_cpp_file);

	this->printer.print_server_file_include(idl_file_name);

	for (const auto& desc : cur_info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		this->printer.print_server_class_impl(cur_info.package_name, desc.block_name);

		for (const auto& rpc : desc.rpcs)
		{
			this->printer.print_server_impl_method(cur_info.package_name,
												   desc.block_name,
												   rpc.method_name,
												   rpc.request_name,
												   rpc.response_name);
		}
		this->printer.print_server_class_impl_end();
	}

	this->printer.print_server_main_begin();
	for (const auto& desc : cur_info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		this->printer.print_server_main_method(desc.block_name);
	}

	this->printer.print_server_main_end();

	this->printer.close();
}

void Generator::generate_client_cpp_file(const idl_info& cur_info, const std::string& idl_file_name)
{
	this->printer.open(this->client_cpp_file);

	this->printer.print_client_file_include(idl_file_name);

	for (const auto& desc : cur_info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		for (const auto& rpc : desc.rpcs)
		{
			this->printer.print_client_done_method(cur_info.package_name,
												   desc.block_name,
												   rpc.method_name,
												   rpc.response_name);
		}
	}

	this->printer.print_client_main_begin();

	for (const auto& desc : cur_info.desc_list)
	{
		if (desc.block_type != "service")
			continue;

		if (this->is_thrift)
			this->printer.print_client_main_service("Thrift",
										cur_info.package_name, desc.block_name);
		else
			this->printer.print_client_main_service("SRPC",
										cur_info.package_name, desc.block_name);
		auto rpc_it = desc.rpcs.cbegin();
		if (rpc_it != desc.rpcs.cend())
		{
			this->printer.print_client_main_method_call(cur_info.package_name,
														desc.block_name,
														rpc_it->method_name,
														rpc_it->request_name);
			rpc_it++;

			//if (rpc_it == desc.rpcs.end())
			//	rpc_it = desc.rpcs.begin();
			//this->printer.print_client_main_create_task(print_client_main_create_task, rpc_it->method_name,
			//											rpc_it->request_name);
		}
	}

	this->printer.print_client_main_end();
	this->printer.close();
}

