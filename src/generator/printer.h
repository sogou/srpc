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

#ifndef __RPC_PRINTER_H__
#define __RPC_PRINTER_H__

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include "descriptor.h"
#include "thrift/rpc_thrift_enum.h"

static inline std::string join_package_names(const std::vector<std::string>& package)
{
	std::string ret = package[0];
	for (size_t i = 1; i < package.size(); i++)
	{
		ret.append("::");
		ret.append(package[i]);
	}
	return ret;
}

static inline std::string change_include_prefix(const std::string& param)
{
	size_t pos = param.find('.');
	if (pos == std::string::npos)
		return param;

	std::string ret = param.substr(0, pos);
	ret.append("::");
	ret.append(param.substr(pos + 1, param.length() - 1 - pos));
	return ret;
}

static inline std::string make_thrift_package_prefix(const std::vector<std::string>& package,
													 const std::string& service,
													 const std::string& param)
{
	if (package.size() == 0)
		return service + "::" + param;

	std::string package_str = join_package_names(package);
	return "::" + package_str + "::" + service + "::" + param;
}

static inline std::string make_package_prefix(const std::vector<std::string>& package,
											  const std::string& param)
{
	size_t pos = param.find('.');
	if (pos != std::string::npos)
		return change_include_prefix(param);

	if (package.size() == 0)
		return param;

	std::string package_str = join_package_names(package);
	return "::" + package_str + "::" + param;
}

static inline std::string make_srpc_service_prefix(const std::vector<std::string>& package,
												   const std::string& service, char sp)
{
	std::string name;

	for (const std::string& p : package)
	{
		name.append(p);
		name.push_back(sp);
	}
	name.append(service);

	return name;
}

static inline std::string make_trpc_service_prefix(const std::vector<std::string>& package,
												   const std::string& service)
{
	if (package.size() == 0)
		return service;

	std::string package_prefix = package[0] + ".";

	for (size_t i = 1; i < package.size(); i++)
		package_prefix = package_prefix + package[i] + ".";

	return package_prefix + service;
}

static inline std::string make_trpc_method_prefix(const std::vector<std::string>& package,
												  const std::string& service,
												  const std::string& method)
{
	std::string method_prefix = "/";

	for (size_t i = 0; i < package.size(); i++)
		method_prefix = method_prefix + package[i] + ".";

	return method_prefix + service + "/" + method;
}

static inline bool is_simple_type(int8_t data_type)
{
	return data_type == srpc::TDT_BOOL
		|| data_type == srpc::TDT_I08
		|| data_type == srpc::TDT_I16
		|| data_type == srpc::TDT_I32
		|| data_type == srpc::TDT_I64
		|| data_type == srpc::TDT_U64
		|| data_type == srpc::TDT_DOUBLE;
}

static inline void fill_thrift_sync_params(const rpc_descriptor& rpc, std::string& return_type, std::string& handler_params, std::string& send_params)
{
	return_type = "void";
	handler_params.clear();
	send_params.clear();
	if (!rpc.resp_params.empty())
	{
		const auto &param = rpc.resp_params[0];
		if (param.field_id == 0)
		{
			if (is_simple_type(param.data_type))
			  return_type = param.type_name;
			else
			{
				handler_params += param.type_name;
				handler_params += "& _return";
			}
		}
	}

	for (const auto &param : rpc.req_params)
	{
		if (!handler_params.empty())
			handler_params += ", ";

		if (!send_params.empty())
			send_params += ", ";

		handler_params += "const ";
		handler_params += param.type_name;
		send_params += "const ";
		send_params += param.type_name;
		if (!is_simple_type(param.data_type))
		{
			handler_params += "&";
			send_params += "&";
		}

		handler_params += " ";
		handler_params += param.var_name;
		send_params += " ";
		send_params += param.var_name;
	}
}

static inline void fill_thrift_async_params(const rpc_descriptor& rpc, std::string& return_type, std::string& handler_params)
{
	return_type.clear();
	handler_params.clear();
	if (!rpc.resp_params.empty())
	{
		const auto &param = rpc.resp_params[0];
		if (param.field_id == 0)
		{
			if (is_simple_type(param.data_type))
			  return_type += "response->result = ";
			else
			  handler_params += "response->result";
		}
	}

	for (const auto &param : rpc.req_params)
	{
		if (!handler_params.empty())
			handler_params += ", ";

		handler_params += "request->";
		handler_params += param.var_name;
	}
}

static void output_descriptor(int& i, FILE *f, const std::string& type_name, size_t& cur, const idl_info& info)
{
	int8_t data_type;
	size_t st = cur;
	std::string key_arg = "void";
	std::string val_arg = "void";

	for (; cur < type_name.size(); cur++)
	{
		if (type_name[cur] == ',' || type_name[cur] == '>' || type_name[cur] == '<')
			break;
	}

	auto cpptype = SGenUtil::strip(type_name.substr(st, cur - st));
	if (info.typedef_mapping.find(cpptype) != info.typedef_mapping.end())
	{
		size_t offset = 0;
		output_descriptor(i,f,info.typedef_mapping.at(cpptype),offset,info);
		return;
	}

	std::string ret;
	if (cpptype == "bool")
		data_type = srpc::TDT_BOOL;
	else if (cpptype == "int8_t")
		data_type = srpc::TDT_I08;
	else if (cpptype == "int16_t")
		data_type = srpc::TDT_I16;
	else if (cpptype == "int32_t")
		data_type = srpc::TDT_I32;
	else if (cpptype == "int64_t")
		data_type = srpc::TDT_I64;
	else if (cpptype == "uint64_t")
		data_type = srpc::TDT_U64;
	else if (cpptype == "double")
		data_type = srpc::TDT_DOUBLE;
	else if (cpptype == "std::string")
		data_type = srpc::TDT_STRING;
	else if (cpptype == "std::map" && cur < type_name.size() && type_name[cur] == '<')
	{
		data_type = srpc::TDT_MAP;
		output_descriptor(i, f, type_name, ++cur, info);
		key_arg = "subtype_" + std::to_string(i);
		output_descriptor(i, f, type_name, ++cur, info);
		val_arg = "subtype_" + std::to_string(i);
		cpptype = SGenUtil::strip(type_name.substr(st, ++cur - st));
	}
	else if (cpptype == "std::set" && cur < type_name.size() && type_name[cur] == '<')
	{
		data_type = srpc::TDT_SET;
		output_descriptor(i, f, type_name, ++cur, info);
		val_arg = "subtype_" + std::to_string(i);
		cpptype = SGenUtil::strip(type_name.substr(st, ++cur - st));
	}
	else if (cpptype == "std::vector" && cur < type_name.size() && type_name[cur] == '<')
	{
		data_type = srpc::TDT_LIST;
		output_descriptor(i, f, type_name, ++cur, info);
		val_arg = "subtype_" + std::to_string(i);
		cpptype = SGenUtil::strip(type_name.substr(st, ++cur - st));
	}
	else
		data_type = srpc::TDT_STRUCT;

	fprintf(f, "\t\tusing subtype_%d = srpc::ThriftDescriptorImpl<%s, %d, %s, %s>;\n",
			++i, cpptype.c_str(), data_type, key_arg.c_str(), val_arg.c_str());
}

class Printer
{
public:
	bool open(std::string out_file)
	{
		this->out_file = fopen(out_file.c_str(), "w");
		return this->out_file;
	}

	void close()
	{
		fclose(this->out_file);
	}

	void print_thrift_include(const idl_info& cur_info)
	{
		fprintf(this->out_file, "%s", this->thrift_include_format.c_str());
		for (const auto& sub_info : cur_info.include_list)
			fprintf(this->out_file, "#include \"%s.h\"\n", sub_info.file_name.c_str());

		//if (cur_info.package_name.length())
		for (size_t i = 0; i < cur_info.package_name.size(); i++)
		{
			fprintf(this->out_file, this->namespace_package_start_format.c_str(),
					cur_info.package_name[i].c_str());
		}
	}

	void print_thrift_struct_declaration(const idl_info& cur_info)
	{
		fprintf(this->out_file,"\n");
		for (const auto& desc : cur_info.desc_list)
		{
			if (desc.block_type == "struct")
				fprintf(this->out_file,"class %s;\n",desc.block_name.c_str());
		}
		fprintf(this->out_file,"\n");
	}

	void print_thrift_typedef(const idl_info& cur_info)
	{
		fprintf(this->out_file,"\n");
		for (const auto& t : cur_info.typedef_list)
		{
			fprintf(this->out_file,"typedef %s %s;\n",t.old_type_name.c_str(),t.new_type_name.c_str());	
		}
		fprintf(this->out_file,"\n");
	}

	void print_thrift_include_enum(const std::string& name, const std::vector<std::string>& enum_lines)
	{
		fprintf(this->out_file, "\nenum %s {\n", name.c_str());
		for (const auto& line : enum_lines)
			fprintf(this->out_file, "\t%s,\n", line.c_str());

		fprintf(this->out_file, "};\n");
	}

	void print_rpc_thrift_struct_class(const std::string& class_name, const std::vector<rpc_param>& class_params,const idl_info& info)
	{
		fprintf(this->out_file, thrift_struct_class_begin_format.c_str(), class_name.c_str());

		for (const auto& ele : class_params)
		{
			if (ele.default_value.empty())
				fprintf(this->out_file, "\t%s %s;\n", ele.type_name.c_str(), ele.var_name.c_str());
			else
				fprintf(this->out_file, "\t%s %s = %s;\n",
						ele.type_name.c_str(), ele.var_name.c_str(), ele.default_value.c_str());
		}

		fprintf(this->out_file, "%s", thrift_struct_class_isset_begin_format.c_str());

		for (const auto& ele : class_params)
		{
			if (ele.required_state != srpc::THRIFT_STRUCT_FIELD_REQUIRED)
			{
				if (!ele.default_value.empty())
					fprintf(this->out_file, "\t\tbool %s = true;\n", ele.var_name.c_str());
				else
					fprintf(this->out_file, "\t\tbool %s = false;\n", ele.var_name.c_str());
			}
		}

		fprintf(this->out_file, thrift_struct_class_isset_end_format.c_str(), class_name.c_str());

		for (const auto& ele : class_params)
		{
			std::string type = ele.type_name;

			if (!is_simple_type(ele.data_type))
				type += '&';

			if (ele.required_state == srpc::THRIFT_STRUCT_FIELD_REQUIRED)
				fprintf(this->out_file, thrift_struct_class_set_required_format.c_str(),
						ele.var_name.c_str(), type.c_str(), ele.var_name.c_str());
			else
				fprintf(this->out_file, thrift_struct_class_set_optional_format.c_str(),
						ele.var_name.c_str(), type.c_str(), ele.var_name.c_str(), ele.var_name.c_str());
		}

		fprintf(this->out_file, thrift_struct_class_operator_equal_begin_format.c_str(), class_name.c_str());
		for (const auto& ele : class_params)
		{
			if (ele.required_state == srpc::THRIFT_STRUCT_FIELD_REQUIRED)
				fprintf(this->out_file, thrift_struct_class_operator_equal_required_format.c_str(),
						ele.var_name.c_str(), ele.var_name.c_str());
			else
				fprintf(this->out_file, thrift_struct_class_operator_equal_optional_format.c_str(),
						ele.var_name.c_str(), ele.var_name.c_str(), ele.var_name.c_str(),
						ele.var_name.c_str(), ele.var_name.c_str());
		}

		fprintf(this->out_file, "%s", thrift_struct_class_operator_equal_end_format.c_str());

		fprintf(this->out_file, thrift_struct_class_elements_start_format.c_str(),
				class_name.c_str(), class_name.c_str(),
				class_name.c_str(), class_name.c_str(),
				class_name.c_str(), class_name.c_str(),
				class_name.c_str(), class_name.c_str(),
				class_name.c_str(), class_name.c_str(), class_name.c_str());

		for (const auto& ele : class_params)
		{
			if (is_simple_type(ele.data_type))
			{
				if (ele.default_value.empty())
					fprintf(this->out_file, "\t\tthis->%s = 0;\n", ele.var_name.c_str());
			}
		}

		fprintf(this->out_file, thrift_struct_class_constructor_end_format.c_str(),
				class_name.c_str(), class_name.c_str());

		fprintf(this->out_file, thrift_struct_element_impl_begin_format.c_str(),
				class_name.c_str(), class_name.c_str());

		int i = 0;
		for (const auto& ele : class_params)
		{
			size_t cur = 0;
			output_descriptor(i, this->out_file, ele.type_name, cur, info);

			std::string p_isset = "0";
			if (ele.required_state != srpc::THRIFT_STRUCT_FIELD_REQUIRED)
				p_isset = "(const char *)(&st->__isset." + ele.var_name + ") - base";

			fprintf(this->out_file, "\t\telements->push_back({subtype_%d::get_instance(), \"%s\", %s, (const char *)(&st->%s) - base, %d, %d});\n",
					i, ele.var_name.c_str(), p_isset.c_str(), ele.var_name.c_str(), ele.field_id, ele.required_state);
		}

		fprintf(this->out_file, thrift_struct_class_end_format.c_str(), class_name.c_str());
	}

	void print_srpc_include(const std::string& prefix, const std::vector<std::string>& package)
	{
		fprintf(this->out_file, this->srpc_include_format.c_str(), prefix.c_str(),
				this->is_thrift ? "thrift" : "pb");

		for (size_t i = 0; i < package.size(); i++)
			fprintf(this->out_file, this->namespace_package_start_format.c_str(),
					package[i].c_str());
	}

	void print_server_file_include(const std::string& prefix)
	{
		fprintf(this->out_file, this->srpc_file_include_format.c_str(), prefix.c_str());
		fprintf(this->out_file, "%s", this->using_namespace_sogou_format.c_str());
		fprintf(this->out_file, "%s", this->wait_group_declaration_format.c_str());
	}

	void print_client_file_include(const std::string& prefix)
	{
		fprintf(this->out_file, this->srpc_file_include_format.c_str(), prefix.c_str());
		fprintf(this->out_file, "%s", this->using_namespace_sogou_format.c_str());
		fprintf(this->out_file, "%s", this->wait_group_declaration_format.c_str());
	}

	void print_server_comment()
	{
		fprintf(this->out_file, "%s", this->server_comment_format.c_str());
	}

	void print_client_comment()
	{
		fprintf(this->out_file, "%s", this->client_comment_format.c_str());
	}

	void print_client_define_done(const std::string& method,
								  const std::string& response)
	{
		std::string resp = change_include_prefix(response);

		fprintf(this->out_file, client_define_done_format.c_str(),
				method.c_str(), resp.c_str());
	}
/*
	void print_client_define_task(const std::string& package, const std::string& method,
								  const std::string& request, const std::string& response)
	{
		std::string req = make_package_prefix(package, request);
		std::string resp = make_package_prefix(package, response);

		fprintf(this->out_file, client_define_task_format.c_str(),
				method.c_str(), req.c_str(), resp.c_str());
	}

	void print_server_method(const std::string& package, const std::string& method,
							 const std::string& request, const std::string& response,
							 const std::string& service)
	{
		std::string req = make_package_prefix(package, request);
		std::string resp = make_package_prefix(package, response);

		fprintf(this->out_file, this->server_method_format.c_str(),
				method.c_str(), req.c_str(), resp.c_str(),
				service.c_str(), method.c_str(),
				req.c_str(), resp.c_str(),
				method.c_str());
	}
*/

	void print_server_class_impl(const std::vector<std::string>& package,
								 const std::string& service)
	{
		std::string base_service = make_package_prefix(package, service);

		fprintf(this->out_file, this->print_server_class_impl_format.c_str(),
				service.c_str(), base_service.c_str());
	}

	void print_server_impl_method(const std::vector<std::string>& package,
								  const std::string& service, const std::string& method,
								  const std::string& request, const std::string& response)
	{
		std::string req;
		std::string resp;

		if (this->is_thrift)
		{
			req = make_thrift_package_prefix(package, service, request);
			resp = make_thrift_package_prefix(package, service, response);
		}
		else
		{
			req = make_package_prefix(package, request);
			resp = make_package_prefix(package, response);
		}

		fprintf(this->out_file, this->server_impl_method_format.c_str(),
				method.c_str(), req.c_str(), resp.c_str());
	}

	void print_server_class_impl_end()
	{
		fprintf(this->out_file, "};");
	}

	void print_server_main_begin()
	{
		fprintf(this->out_file, "%s", this->server_main_begin_format.c_str());

		if (this->is_thrift)
			fprintf(this->out_file, this->server_main_ip_port_format.c_str(), "Thrift");
		else
		{
			fprintf(this->out_file, "%s", this->main_pb_version_format.c_str());
			fprintf(this->out_file, this->server_main_ip_port_format.c_str(), "SRPC");
		}
	}

	void print_server_main_method(const std::string& service)
	{
		std::string service_lower = service;
		std::transform(service_lower.begin(), service_lower.end(),
					   service_lower.begin(), ::tolower);
	
		fprintf(this->out_file, this->server_main_method_format.c_str(),
				service.c_str(), service_lower.c_str(), service_lower.c_str());
	}

	void print_server_main_end()
	{
		fprintf(this->out_file, "%s", this->server_main_end_format.c_str());
		if (!this->is_thrift)
			fprintf(this->out_file, "%s", this->main_pb_shutdown_format.c_str());
		fprintf(this->out_file, "%s", this->main_end_return_format.c_str());
	}

	void print_server_class_thrift(const std::string& service, const std::vector<rpc_descriptor>& rpcs)
	{
		std::string return_type;
		std::string handler_params;
		std::string send_params;
		fprintf(this->out_file, "%s", this->server_class_constructor_format.c_str());

		for (const auto& rpc : rpcs)
		{
			fill_thrift_sync_params(rpc, return_type, handler_params, send_params);
			fprintf(this->out_file, this->server_class_method_declaration_thrift_handler_format.c_str(),
					return_type.c_str(), rpc.method_name.c_str(), handler_params.c_str());
		}

		fprintf(this->out_file, "\npublic:\n");
		for (const auto& rpc : rpcs)
		{
			fill_thrift_async_params(rpc, return_type, handler_params);
			fprintf(this->out_file, this->server_class_method_declaration_thrift_format.c_str(),
					rpc.method_name.c_str(), rpc.request_name.c_str(), rpc.response_name.c_str());
		}

		fprintf(this->out_file, "%s", this->server_class_end_format.c_str());
	}

	void print_server_methods_thrift(const std::string& service, const std::vector<rpc_descriptor>& rpcs)
	{
		std::string return_type;
		std::string handler_params;
		std::string send_params;
		for (const auto& rpc : rpcs)
		{
			fill_thrift_sync_params(rpc, return_type, handler_params, send_params);
			fprintf(this->out_file, this->server_class_method_implementation_thrift_handler_format.c_str(),
					return_type.c_str(), rpc.method_name.c_str(), handler_params.c_str(),
					return_type == "void" ? " " : " return 0; ");
		}

		for (const auto& rpc : rpcs)
		{
			fill_thrift_async_params(rpc, return_type, handler_params);
			fprintf(this->out_file, this->server_class_method_implementation_thrift_format.c_str(),
					rpc.method_name.c_str(), rpc.request_name.c_str(), rpc.response_name.c_str(),
					return_type.c_str(), rpc.method_name.c_str(), handler_params.c_str());
		}
	}

	void print_server_class(const std::string& service,
							const std::vector<rpc_descriptor>& rpcs)
	{
		fprintf(this->out_file, "%s", this->server_class_constructor_format.c_str());

		for (const auto& rpc : rpcs)
		{
			std::string req = change_include_prefix(rpc.request_name);
			std::string resp = change_include_prefix(rpc.response_name);

			fprintf(this->out_file, this->server_class_method_format.c_str(),
					rpc.method_name.c_str(), req.c_str(),
					resp.c_str());
		}

		fprintf(this->out_file, "%s", this->server_class_end_format.c_str());
	}

	void print_server_methods(const std::string& service, const std::vector<rpc_descriptor>& rpcs)
	{
		for (const auto& rpc : rpcs)
		{
			fprintf(this->out_file, this->server_methods_format.c_str(),
					service.c_str(), service.c_str(),
					rpc.method_name.c_str(), rpc.method_name.c_str(),
					rpc.method_name.c_str());
		}
	}

	void print_client_class(const std::string& type,
							const std::string& service,
							const std::vector<rpc_descriptor>& rpcs)
	{
		fprintf(this->out_file, this->client_class_constructor_start_format.c_str(),
				type.c_str(), type.c_str());

		if (this->is_thrift)
		{
			std::string return_type;
			std::string handler_params;
			std::string send_params;
			std::string recv_params;
			for (const auto& rpc : rpcs)
			{
				fill_thrift_sync_params(rpc, return_type, handler_params, send_params);
				if (return_type != "void")
					recv_params.clear();
				else if (!rpc.resp_params.empty() && rpc.resp_params[0].field_id == 0)
					recv_params = rpc.resp_params[0].type_name + "& _return";
				else
					recv_params.clear();

				fprintf(this->out_file,
						this->client_class_construct_methods_thrift_format.c_str(),
						return_type.c_str(), rpc.method_name.c_str(), handler_params.c_str(),
						rpc.method_name.c_str(), send_params.c_str(),
						return_type.c_str(), rpc.method_name.c_str(), recv_params.c_str());
			}

			fprintf(this->out_file, "%s", this->client_class_get_last_thrift_format.c_str());

			fprintf(this->out_file, "public:\n");
		}

		for (const auto& rpc : rpcs)
		{
			std::string req = change_include_prefix(rpc.request_name);
			std::string resp = change_include_prefix(rpc.response_name);

			fprintf(this->out_file,
					this->client_class_construct_methods_format.c_str(),
					rpc.method_name.c_str(), req.c_str(), rpc.method_name.c_str(),
					rpc.method_name.c_str(), req.c_str(), resp.c_str(),
					resp.c_str(), rpc.method_name.c_str(), req.c_str());
		}

		fprintf(this->out_file, this->client_class_constructor_format.c_str(),
				type.c_str(), type.c_str());

		for (const auto& rpc : rpcs)
		{
			fprintf(this->out_file,
					this->client_class_create_methods_format.c_str(),
					type.c_str(), rpc.method_name.c_str(), rpc.method_name.c_str());
					//type.c_str(), rpc.method_name.c_str(), rpc.request_name.c_str(), rpc.method_name.c_str());
		}

		if (this->is_thrift)
		{
			fprintf(this->out_file, "%s", client_class_private_get_thrift_format.c_str());
			for (const auto& rpc : rpcs)
			{
				std::string resp = change_include_prefix(rpc.response_name);
				fprintf(this->out_file,
						this->client_class_get_rpc_thread_resp_thrift_format.c_str(),
						resp.c_str(), rpc.method_name.c_str());
			}
		}

		fprintf(this->out_file, this->client_class_constructor_end_format.c_str(),
				service.c_str());
	}

	void print_implement_comments()
	{
		fprintf(this->out_file, "\n///// implements detials /////\n");
	}

	void print_server_constructor(const std::string& service, const std::vector<rpc_descriptor>& rpcs)
	{
		fprintf(this->out_file, this->server_constructor_method_format.c_str(),
				service.c_str());

		for (const auto& rpc : rpcs)
		{
			fprintf(this->out_file, this->server_constructor_add_method_format.c_str(),
					rpc.method_name.c_str(), rpc.method_name.c_str());
		}
		fprintf(this->out_file, "}\n");
	}

	void print_client_constructor(const std::string& type, const std::string& service,
								  const std::vector<std::string>& package)
	{
		bool is_srpc_thrift = (this->is_thrift && (type == "SRPC" || type == "SRPCHttp"));
		const char *method_ip = is_srpc_thrift ? client_constructor_methods_ip_srpc_thrift_format.c_str() : "";
		const char *method_params = is_srpc_thrift ? client_constructor_methods_params_srpc_thrift_format.c_str() : "";

		std::string full_service = service;

		if (type == "TRPC")
			full_service = make_trpc_service_prefix(package, service);
		else if (type == "SRPC" || type == "BRPC" || type == "TRPCHttp")
			full_service = make_srpc_service_prefix(package, service, '.');
		else if (type == "SRPCHttp")
			full_service = make_srpc_service_prefix(package, service, '/');

		fprintf(this->out_file, this->client_constructor_methods_format.c_str(),
				type.c_str(), type.c_str(),
				type.c_str(), full_service.c_str(),
				method_ip, type.c_str(),

				type.c_str(), type.c_str(),
				type.c_str(), full_service.c_str(),
				method_params, type.c_str());
	}

	void print_client_methods(const std::string& type,
							  const std::string& service,
							  const std::vector<rpc_descriptor>& rpcs,
							  const std::vector<std::string>& package)
	{
		for (const auto& rpc : rpcs)
		{
			std::string req = change_include_prefix(rpc.request_name);
			std::string resp = change_include_prefix(rpc.response_name);

			if (type == "TRPC")
			{
				std::string full_method = make_trpc_method_prefix(package,
																  service,
																  rpc.method_name);

				fprintf(this->out_file, this->client_method_trpc_format.c_str(),
						type.c_str(), rpc.method_name.c_str(),
						req.c_str(), rpc.method_name.c_str(),
						full_method.c_str(),

						type.c_str(), rpc.method_name.c_str(),
						req.c_str(), resp.c_str(), rpc.method_name.c_str(),

						resp.c_str(), type.c_str(),
						rpc.method_name.c_str(), req.c_str(),
						resp.c_str(), resp.c_str(), full_method.c_str(),
						resp.c_str());
			}
			else if (type == "TRPCHttp")
			{
				fprintf(this->out_file, this->client_method_trpc_format.c_str(),
						type.c_str(), rpc.method_name.c_str(),
						req.c_str(), rpc.method_name.c_str(),
						rpc.method_name.c_str(),

						type.c_str(), rpc.method_name.c_str(),
						req.c_str(), resp.c_str(), rpc.method_name.c_str(),

						resp.c_str(), type.c_str(),
						rpc.method_name.c_str(), req.c_str(),
						resp.c_str(), resp.c_str(), rpc.method_name.c_str(),
						resp.c_str());
			}
			else
			{
				fprintf(this->out_file, this->client_method_format.c_str(),
						type.c_str(), rpc.method_name.c_str(),
						req.c_str(), rpc.method_name.c_str(),
						rpc.method_name.c_str(),

						type.c_str(), rpc.method_name.c_str(),
						req.c_str(), resp.c_str(), rpc.method_name.c_str(),

						resp.c_str(), type.c_str(),
						rpc.method_name.c_str(), req.c_str(),
						resp.c_str(), resp.c_str(), rpc.method_name.c_str(),
						resp.c_str());
			}
		}

		if (this->is_thrift)
		{
			std::string return_type;
			std::string handler_params;
			std::string send_params;
			std::string recv_params;
			for (const auto& rpc : rpcs)
			{
				std::string req = change_include_prefix(rpc.request_name);
				std::string resp = change_include_prefix(rpc.response_name);
				std::string last_return;

				fill_thrift_sync_params(rpc, return_type, handler_params, send_params);
				if (return_type != "void")
					last_return = "return __thrift__sync__resp.result";
				else if (!rpc.resp_params.empty() && rpc.resp_params[0].field_id == 0)
					last_return = "_return = std::move(__thrift__sync__resp.result)";
				else
					last_return = "(void)__thrift__sync__resp";

				fprintf(this->out_file,
						this->client_method_thrift_begin_format.c_str(),
						resp.c_str(), type.c_str(), rpc.method_name.c_str(), resp.c_str(),
						return_type.c_str(), type.c_str(), rpc.method_name.c_str(), handler_params.c_str(),
						req.c_str(), resp.c_str());

				for (const auto &param : rpc.req_params)
					fprintf(this->out_file, "\t__thrift__sync__req.%s = %s;\n", param.var_name.c_str(), param.var_name.c_str());

				fprintf(this->out_file,
						this->client_method_thrift_end_format.c_str(),
						rpc.method_name.c_str(), type.c_str(), last_return.c_str(),
						type.c_str(), rpc.method_name.c_str(), send_params.c_str(), req.c_str());

				for (const auto &param : rpc.req_params)
					fprintf(this->out_file, "\t__thrift__sync__req.%s = %s;\n", param.var_name.c_str(), param.var_name.c_str());

				if (return_type != "void")
				{
					last_return = "return std::move(res.result);";
					recv_params.clear();
				}
				else if (!rpc.resp_params.empty() && rpc.resp_params[0].field_id == 0)
				{
					last_return = "_return = std::move(res.result)";
					recv_params = rpc.resp_params[0].type_name + "& _return";
				}
				else
				{
					last_return = "(void)res";
					recv_params.clear();
				}

				fprintf(this->out_file,
						this->cilent_method_thrift_send_end_format.c_str(),
						resp.c_str(), rpc.method_name.c_str(), resp.c_str(), rpc.method_name.c_str(),

						return_type.c_str(), type.c_str(), rpc.method_name.c_str(), recv_params.c_str(),
						rpc.method_name.c_str(), last_return.c_str());
			}

			fprintf(this->out_file, this->client_method_get_last_thrift_format.c_str(),
					type.c_str(), type.c_str(), type.c_str());
		}
	}

	void print_client_create_task(const std::string& type, const std::string& service,
								  const std::vector<rpc_descriptor>& rpcs,
								  const std::vector<std::string>& package)
	{
		for (const auto& rpc : rpcs)
		{
			if (type == "TRPC")
			{
				std::string full_method = make_trpc_method_prefix(package,
																  service,
																  rpc.method_name);

				fprintf(this->out_file, this->client_create_task_trpc_format.c_str(),
						type.c_str(), type.c_str(), rpc.method_name.c_str(),
				rpc.method_name.c_str(), full_method.c_str());
			}
			else if (type == "TRPCHttp")
			{
				fprintf(this->out_file, this->client_create_task_trpc_format.c_str(),
						type.c_str(), type.c_str(), rpc.method_name.c_str(),
						rpc.method_name.c_str(), rpc.method_name.c_str());
			}
			else
			{
				fprintf(this->out_file, this->client_create_task_format.c_str(),
						type.c_str(), type.c_str(), rpc.method_name.c_str(),
						rpc.method_name.c_str(), rpc.method_name.c_str());
			}
		}
	}

	void print_service_namespace(const std::string& service)
	{
		fprintf(this->out_file, this->namespace_service_start_format.c_str(),
				service.c_str());
	}

	void print_service_namespace_end(const std::string& service)
	{
		fprintf(this->out_file, this->namespace_service_end_format.c_str(),
				service.c_str());			
	}

	void print_create_client_controller(const std::string& service,
									  	const std::vector<rpc_descriptor>& rpcs)
	{
		for (const auto& rpc : rpcs)
		{
			this->print_create_method_controller(service, rpc.method_name);
		}
	}

	void print_create_method_controller(const std::string& service, const std::string& method)
	{
		std::string method_lower = method;
		std::transform(method_lower.begin(), method_lower.end(),
						method_lower.begin(), ::tolower);
		fprintf(this->out_file, this->namespace_method_task_format.c_str(),
				//create_method_cntl(stub)
				method.c_str(), method_lower.c_str(),
				service.c_str(), method.c_str(),
				method.c_str(), method.c_str());
	}

	void print_client_done_method(const std::vector<std::string>& package,
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

		fprintf(this->out_file, this->client_done_format.c_str(),
				method_lower.c_str(), resp.c_str(),
				service.c_str(), method.c_str());
	}

	void print_client_main_begin()
	{
		fprintf(this->out_file, "%s", this->client_main_begin_format.c_str());
		if (!this->is_thrift)
			fprintf(this->out_file, "%s", this->main_pb_version_format.c_str());
		fprintf(this->out_file, "%s", this->client_main_ip_port_format.c_str());
	}

	void print_client_main_service(const std::string& type,
								   const std::vector<std::string>& package,
								   const std::string& service)
	{
		//std::string service_lower = service;
		//std::transform(service_lower.begin(), service_lower.end(),
		//				service_lower.begin(), ::tolower);

		std::string base_service = make_package_prefix(package, service);
		fprintf(this->out_file, this->client_main_service_format.c_str(),
				base_service.c_str(), type.c_str());
	}

	void print_client_main_method_call(const std::vector<std::string>& package,
									   const std::string& service,
									   const std::string& method,
									   const std::string& request)
	{
		std::string method_lower = method;
		std::transform(method_lower.begin(), method_lower.end(),
						method_lower.begin(), ::tolower);

		std::string req;

		if (this->is_thrift)
			req = make_thrift_package_prefix(package, service, request);
		else
			req = make_package_prefix(package, request);

		fprintf(this->out_file, this->client_main_method_call_format.c_str(),
				req.c_str(), method_lower.c_str(), method_lower.c_str(),
				method.c_str(), method_lower.c_str(), method_lower.c_str()); 
	}

/*
	void print_client_main_create_task(const std::string& package, const std::string& service,
									   const std::string& method, const std::string& request)
	{
		std::string method_lower = method;
		std::transform(method_lower.begin(), method_lower.end(),
						method_lower.begin(), ::tolower);

		std::string req = make_package_prefix(package, request);

		fprintf(this->out_file, this->client_main_create_task_format.c_str(),
				req.c_str(), method_lower.c_str(), service.c_str(),
				method.c_str(), method_lower.c_str(), method.c_str(),
				method_lower.c_str(), method_lower.c_str(),
				method_lower.c_str(), method_lower.c_str());
	}
*/

	void print_client_main_end()
	{
		fprintf(this->out_file, "%s", this->client_main_end_format.c_str());
		if (!this->is_thrift)
			fprintf(this->out_file, "%s", this->main_pb_shutdown_format.c_str());
		fprintf(this->out_file, "%s", this->main_end_return_format.c_str());
	}

	void print_end(const std::vector<std::string>& package)
	{
		for (size_t i = package.size() - 1; i != (size_t)-1; i--)
			fprintf(this->out_file, namespace_package_end_format.c_str(),
					package[i].c_str());
//		fprintf(this->out_file, "} // namespace srpc\n");
	}

	void print_empty_line()
	{
		fprintf(this->out_file, "\n");
	}

	Printer(bool is_thrift) { this->is_thrift = is_thrift; }

private:
	FILE *out_file;
	bool is_thrift;

	std::string thrift_include_format = R"(#pragma once
#include "srpc/rpc_thrift_idl.h"
)";

	std::string srpc_include_format = R"(#pragma once
#include <stdio.h>
#include <string>
#include "srpc/rpc_define.h"
#include "%s.%s.h"
)";

	std::string thrift_include_package_format = R"(
namespace %s
{
)";

	std::string srpc_include_package_format = R"(
using namespace %s;
)";

	std::string srpc_file_include_format = R"(
#include "%s.srpc.h"
#include "workflow/WFFacilities.h"
)";

	std::string using_namespace_sogou_format = R"(
using namespace srpc;
)";

	std::string wait_group_declaration_format = R"(
static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}
)";

/*
	std::string namespace_total_start_format = R"(
namespace srpc
{
)";
*/

	std::string namespace_package_start_format = R"(
namespace %s
{
)";

	std::string namespace_package_end_format = R"(} // end namespace %s

)";

	std::string server_comment_format = R"(
/*
 * Server codes
 * Generated by SRPC
 */
)";

	std::string client_comment_format = R"(
/*
 * Client codes
 * Generated by SRPC
 */
)";

	std::string client_define_done_format = R"(
using %sDone = std::function<void (%s *, srpc::RPCContext *)>;)";

	std::string client_define_task_format = R"(using %sTask = srpc::RPCClientTask<%s, %s>;
)";

	std::string server_context_format = R"(
using %sContext = srpc::RPCContext<%s, %s>;
)";

	std::string server_controller_format = R"(
class Server%sCntl : public srpc::%sCntl
{
public:
	// must implement this in server codes
	void %s(%s *request, %s *response);

	Server%sCntl(srpc::RPCTask *task)
		: %sCntl(task)
	{}
};
)";

	std::string client_controller_format = R"(

class Client%sCntl;

typedef std::function<void (Client%sCntl *, %s *)> %sDone;

class Client%sCntl : public srpc::%sCntl
{
public:
	Client%sCntl(srpc::RPCTask *task)
		: %sCntl(task)
	{}

	void %s(%sDone done, SeriesWork *series)
	{
		this->done = std::move(done);
		if (series != NULL)
		{
			this->set_series(series);
			series->push_back(this->get_task());
		}
		this->service->call_method("%s", this);
	}

	%sDone done;
};
)";

	std::string server_method_format = R"(
void %s(srpc::RPCTask *task, %s *request, %s *response);

static inline void %s%s(srpc::RPCTask *task, \
rpc_buf_t *req_buf, rpc_buf_t *resp_buf)
{
	%s pb_req;
	%s pb_resp;
	pb_req.ParseFromArray(req_buf->buf, req_buf->len);
	%s(task, &pb_req, &pb_resp);
	resp_buf->buf = malloc(pb_resp.ByteSize());
	resp_buf->len = pb_resp.ByteSize();
	pb_resp.SerializeToArray(resp_buf->buf, resp_buf->len);
}
)";

	std::string server_class_constructor_format = R"(
class Service : public srpc::RPCService
{
public:
	// please implement these methods in server.cc
)";

	std::string server_class_end_format = R"(
public:
	Service();
};
)";

	std::string server_class_method_format = R"(
	virtual void %s(%s *request, %s *response,
					srpc::RPCContext *ctx) = 0;
)";

	std::string server_class_method_declaration_thrift_format = R"(
	virtual void %s(%s *request, %s *response,
					srpc::RPCContext *ctx);
)";

	std::string server_class_method_implementation_thrift_format = R"(
inline void Service::%s(%s *request, %s *response, srpc::RPCContext *ctx)
{
	%sthis->%s(%s);
}
)";

	std::string server_class_method_declaration_thrift_handler_format = R"(
	virtual %s %s(%s);
)";

	std::string server_class_method_implementation_thrift_handler_format = R"(
inline %s Service::%s(%s) {%s}
)";

	std::string server_class_construct_method_format = R"(

	void %s%s(srpc::RPCTask *task);
)";

	std::string server_constructor_method_format = R"(
inline Service::Service(): srpc::RPCService("%s")
{)";

	std::string server_constructor_add_method_format = R"(
	this->srpc::RPCService::add_method("%s",
		[this](srpc::RPCWorker& worker) ->int {
			return ServiceRPCCallImpl(this, worker, &Service::%s);
		});
)";

	std::string server_methods_format = R"(
void %s::%s%s(srpc::RPCTask *task)
{
	Server%sCntl cntl(task);
	srpc::RPCService::parse_from_buffer(task, cntl.get_input(), true);
	cntl.%s(cntl.get_req(), cntl.get_resp());
	srpc::RPCService::serialize_to_buffer(task, cntl.get_output(), false);
}
)";

	std::string client_class_constructor_start_format = R"(
class %sClient : public srpc::%sClient
{
public:)";

	std::string client_constructor_methods_format = R"(
inline %sClient::%sClient(const char *host, unsigned short port):
	srpc::%sClient("%s")
{
	struct srpc::RPCClientParams params = srpc::RPC_CLIENT_PARAMS_DEFAULT;
	%s
	params.host = host;
	params.port = port;
	this->srpc::%sClient::init(&params);
}

inline %sClient::%sClient(const struct srpc::RPCClientParams *params):
	srpc::%sClient("%s")
{
	const struct srpc::RPCClientParams *temp = params;
	struct srpc::RPCClientParams temp_params;
	%s
	this->srpc::%sClient::init(temp);
}
)";

	std::string client_constructor_methods_ip_srpc_thrift_format = R"(
	params.task_params.data_type = srpc::RPCDataThrift;
)";

	std::string client_constructor_methods_params_srpc_thrift_format = R"(
	if (params->task_params.data_type == srpc::RPCDataUndefined)
	{
		temp_params = *temp;
		temp_params.task_params.data_type = srpc::RPCDataThrift;
		temp = &temp_params;
	}
)";

	std::string client_class_constructor_ip_port_format = R"(
	%sClient(const char *host, unsigned short port) :
			srpc::RPCService("%s")
	{
		this->params.service_params.host = host;
		this->params.service_params.port = port;
		this->register_compress_type(srpc::RPCCompressGzip);
)";

	std::string client_class_constructor_params_format = R"(
	%sClient(struct srpc::RPCServiceParams& params) :
			srpc::RPCService("%s")
	{
		this->params.service_params = params;

		if (!params.url.empty())
		{
			URIParser::parse(params.url, this->params.uri);
			this->params.has_uri = true;
		} else {
			this->params.has_uri = false;
		}

		if (!params.host.empty() && !params.port.empty())
		{
			get_addr_info(params.host.c_str(), params.port.c_str(), &this->params.ai);
			this->params.has_addr_info = true;
		} else {
			this->params.has_addr_info = false;
		}

		this->register_compress_type(srpc::RPCCompressGzip);
)";

	std::string client_class_functions = R"(
	const struct srpc::RPCClientParams *get_params()
	{
		return &this->params;
	}
)";

	std::string client_class_variables = R"(
	struct srpc::RPCClientParams params;
)";

	std::string tab_right_braket = R"(
	}
)";

	std::string client_class_done_format = R"(
	%sDone %s_done;
)";

	std::string client_class_construct_methods_thrift_format = R"(
	%s %s(%s);
	void send_%s(%s);
	%s recv_%s(%s);
)";

	std::string client_class_get_last_thrift_format = R"(
	bool thrift_last_sync_success() const;
	const srpc::RPCSyncContext& thrift_last_sync_ctx() const;
)";

	std::string client_class_private_get_thrift_format = R"(
private:
	static srpc::RPCSyncContext *get_thread_sync_ctx();
)";

	std::string client_class_get_rpc_thread_resp_thrift_format = R"(
	static srpc::ThriftReceiver<%s> *get_thread_sync_receiver_%s();
)";

	std::string client_class_construct_methods_format = R"(
	void %s(const %s *req, %sDone done);
	void %s(const %s *req, %s *resp, srpc::RPCSyncContext *sync_ctx);
	WFFuture<std::pair<%s, srpc::RPCSyncContext>> async_%s(const %s *req);
)";

	std::string client_class_constructor_format = R"(
public:
	%sClient(const char *host, unsigned short port);
	%sClient(const struct srpc::RPCClientParams *params);

public:
)";

	std::string client_class_create_methods_format = R"(	srpc::%sClientTask *create_%s_task(%sDone done);
)";
//%sClientTask *create_%s_task(%s *req, %sDone done);

	std::string client_class_constructor_end_format = R"(};
)";

	std::string client_method_format = R"(
inline void %sClient::%s(const %s *req, %sDone done)
{
	auto *task = this->create_rpc_client_task("%s", std::move(done));

	task->serialize_input(req);
	task->start();
}

inline void %sClient::%s(const %s *req, %s *resp, srpc::RPCSyncContext *sync_ctx)
{
	auto res = this->async_%s(req).get();

	if (resp && res.second.success)
		*resp = std::move(res.first);

	if (sync_ctx)
		*sync_ctx = std::move(res.second);
}

inline WFFuture<std::pair<%s, srpc::RPCSyncContext>> %sClient::async_%s(const %s *req)
{
	using RESULT = std::pair<%s, srpc::RPCSyncContext>;
	auto *pr = new WFPromise<RESULT>();
	auto fr = pr->get_future();
	auto *task = this->create_rpc_client_task<%s>("%s", srpc::RPCAsyncFutureCallback<%s>);

	task->serialize_input(req);
	task->user_data = pr;
	task->start();
	return fr;
}
)";

	std::string client_method_trpc_format = R"(
inline void %sClient::%s(const %s *req, %sDone done)
{
	auto *task = this->create_rpc_client_task("%s", std::move(done));

	if (!this->params.caller.empty())
		task->get_req()->set_caller_name(this->params.caller);
	task->serialize_input(req);
	task->start();
}

inline void %sClient::%s(const %s *req, %s *resp, srpc::RPCSyncContext *sync_ctx)
{
	auto res = this->async_%s(req).get();

	if (resp && res.second.success)
		*resp = std::move(res.first);

	if (sync_ctx)
		*sync_ctx = std::move(res.second);
}

inline WFFuture<std::pair<%s, srpc::RPCSyncContext>> %sClient::async_%s(const %s *req)
{
	using RESULT = std::pair<%s, srpc::RPCSyncContext>;
	auto *pr = new WFPromise<RESULT>();
	auto fr = pr->get_future();
	auto *task = this->create_rpc_client_task<%s>("%s", srpc::RPCAsyncFutureCallback<%s>);

	if (!this->params.caller.empty())
		task->get_req()->set_caller_name(this->params.caller);
	task->serialize_input(req);
	task->user_data = pr;
	task->start();
	return fr;
}
)";

	std::string client_method_thrift_begin_format = R"(
inline srpc::ThriftReceiver<%s> *%sClient::get_thread_sync_receiver_%s()
{
	static thread_local srpc::ThriftReceiver<%s> receiver;
	return &receiver;
}

inline %s %sClient::%s(%s)
{
	%s __thrift__sync__req;
	%s __thrift__sync__resp;
)";

	std::string client_method_thrift_end_format = R"(
	%s(&__thrift__sync__req, &__thrift__sync__resp, %sClient::get_thread_sync_ctx());
	%s;
}

inline void %sClient::send_%s(%s)
{
	%s __thrift__sync__req;
)";

	std::string cilent_method_thrift_send_end_format = R"(
	auto *task = this->create_rpc_client_task<%s>("%s", srpc::ThriftSendCallback<%s>);

	task->serialize_input(&__thrift__sync__req);
	task->user_data = get_thread_sync_receiver_%s();
	task->start();
}

inline %s %sClient::recv_%s(%s)
{
	bool in_handler = false;
	auto *receiver = get_thread_sync_receiver_%s();
	std::unique_lock<std::mutex> lock(receiver->mutex);

	if (!receiver->is_done)
	{
		in_handler = WFGlobal::get_scheduler()->is_handler_thread();
		if (in_handler)
			WFGlobal::sync_operation_begin();

		while (!receiver->is_done)
			receiver->cond.wait(lock);
	}

	receiver->is_done = false;
	*get_thread_sync_ctx() = std::move(receiver->ctx);
	auto res = std::move(receiver->output);
	lock.unlock();
	if (in_handler)
		WFGlobal::sync_operation_end();

	%s;
}
)";

	std::string client_method_get_last_thrift_format = R"(
inline srpc::RPCSyncContext *%sClient::get_thread_sync_ctx()
{
	static thread_local srpc::RPCSyncContext thread_sync_ctx;
	return &thread_sync_ctx;
}

inline bool %sClient::thrift_last_sync_success() const
{
	return get_thread_sync_ctx()->success;
}

inline const srpc::RPCSyncContext& %sClient::thrift_last_sync_ctx() const
{
	return *get_thread_sync_ctx();
}

)";

	std::string client_create_task_format = R"(
inline srpc::%sClientTask *%sClient::create_%s_task(%sDone done)
{
	return this->create_rpc_client_task("%s", std::move(done));
}
)";

	std::string client_create_task_trpc_format = R"(
inline srpc::%sClientTask *%sClient::create_%s_task(%sDone done)
{
	auto *task = this->create_rpc_client_task("%s", std::move(done));

	if (!this->params.caller.empty())
		task->get_req()->set_caller_name(this->params.caller);

	return task;
}
)";

/*
inline %sClientTask *%sClient::create_%s_task(%s *req, %sDone done)
{
	auto *task = this->create_rpc_client_task("%s", std::move(done));

	task->serialize_input(req);
	return task;
}*/

	std::string namespace_service_start_format = R"(
namespace %s
{
)";

	std::string namespace_service_end_format = R"(
} // end namespace %s

)";

	std::string namespace_method_task_format = R"(
static Client%sCntl *create_%s_cntl(%sClient *stub)
{
	rpc_callback_t cb = std::bind(&%sClient::%sCallback,
								stub, std::placeholders::_1);
	auto *task = new ComplexRPCClientTask(stub->get_params(), std::move(cb));

	if (task->get_error())
	{
		errno = task->get_error();
		delete task;
		return NULL;
	}

	Client%sCntl *cntl = new Client%sCntl(task);
	cntl->set_service(stub);
	task->set_controller(cntl);
	return cntl;
}
)";

	std::string print_server_class_impl_format = R"(
class %sServiceImpl : public %s::Service
{
public:
)";

	std::string server_impl_method_format = R"(
	void %s(%s *request, %s *response, srpc::RPCContext *ctx) override
	{}
)";

	std::string server_main_begin_format = R"(

int main()
{)";
	std::string server_main_ip_port_format = R"(
	unsigned short port = 1412;
	%sServer server;
)";

	std::string server_main_method_format = R"(
	%sServiceImpl %s_impl;
	server.add_service(&%s_impl);
)";

	std::string server_main_end_format = R"(
	server.start(port);
	wait_group.wait();
	server.stop();)";

	std::string client_done_format = R"(
static void %s_done(%s *response, srpc::RPCContext *context)
{
}
)";

	std::string main_pb_version_format = R"(
	GOOGLE_PROTOBUF_VERIFY_VERSION;)";

	std::string client_main_begin_format = R"(
int main()
{)";
	std::string client_main_ip_port_format = R"(
	const char *ip = "127.0.0.1";
	unsigned short port = 1412;
)";

	std::string client_main_service_format = R"(
	%s::%sClient client(ip, port);
)";

	std::string client_main_method_call_format = R"(
	// example for RPC method call
	%s %s_req;
	//%s_req.set_message("Hello, srpc!");
	client.%s(&%s_req, %s_done);
)";

/*
	std::string client_main_create_task_format = R"(
	// example for creating RPC task
	%s %s_req;
	srpc::RPC%s::%sTask *%s_task = client.create_%s_task(%s_done);
	%s_task->serialize_input(&%s_req);
	%s_task->start();
)";
*/

	std::string client_main_end_format = R"(
	wait_group.wait();)";
	std::string main_pb_shutdown_format = R"(
	google::protobuf::ShutdownProtobufLibrary();)";
	std::string main_end_return_format = R"(
	return 0;
}
)";
	std::string thrift_struct_class_begin_format = R"(
class %s : public srpc::ThriftIDLMessage
{
public:
)";
	std::string thrift_struct_class_constructor_end_format = R"(
		this->elements = srpc::ThriftElementsImpl<%s>::get_elements_instance();
		this->descriptor = srpc::ThriftDescriptorImpl<%s, srpc::TDT_STRUCT, void, void>::get_instance();
	}
)";
	std::string thrift_struct_class_end_format = R"(
	}
	friend class srpc::ThriftElementsImpl<%s>;
};
)";
	std::string thrift_struct_element_impl_begin_format = R"(
private:
	static void StaticElementsImpl(std::list<srpc::struct_element> *elements)
	{
		const %s *st = (const %s *)0;
		const char *base = (const char *)st;
		(void)base;
)";
	std::string thrift_struct_class_set_required_format = R"(
	void __set_%s(const %s val)
	{
		this->%s = val;
	}
)";
	std::string thrift_struct_class_set_optional_format = R"(
	void __set_%s(const %s val)
	{
		this->%s = val;
		this->__isset.%s = true;
	}
)";
	std::string thrift_struct_class_isset_begin_format = R"(
public:
	struct ISSET
	{
)";
	std::string thrift_struct_class_isset_end_format = R"(
	} __isset;
)";
	std::string thrift_struct_class_operator_equal_begin_format = R"(
	bool operator == (const %s& rhs) const
	{
)";
	std::string thrift_struct_class_operator_equal_required_format = R"(
		if (!(this->%s == rhs.%s))
			return false;
)";
	std::string thrift_struct_class_operator_equal_optional_format = R"(
		if (this->__isset.%s != rhs.__isset.%s)
			return false;
		else if (this->__isset.%s && !(this->%s == rhs.%s))
			return false;
)";
	std::string thrift_struct_class_operator_equal_end_format = R"(
		return true;
	}
)";
	std::string thrift_struct_class_elements_start_format = R"(
	bool operator != (const %s& rhs) const
	{
		return !(*this == rhs);
	}

	bool operator < (const %s & ) const;

	%s(const %s&) = default;
	%s(%s&&) = default;
	%s& operator= (const %s&) = default;
	%s& operator= (%s&&) = default;
	%s()
	{
)";
};

#endif

