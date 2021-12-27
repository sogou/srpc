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

#ifndef __RPC_DESCRIPTOR_H__
#define __RPC_DESCRIPTOR_H__

#include <vector>
#include <string>
#include <set>

struct rpc_param
{
	std::string type_name;
	std::string var_name;
	std::string default_value;
	int16_t field_id;
	int8_t data_type;
	int8_t required_state;
};

struct rpc_descriptor
{
	std::string method_name;
	std::string request_name;
	std::string response_name;
	std::vector<rpc_param> req_params;
	std::vector<rpc_param> resp_params;
};

struct struct_descriptor
{
	std::vector<rpc_param> params;
};

struct typedef_descriptor
{
	std::string old_type_name;
	std::string new_type_name;
};

struct Descriptor
{
	std::string block_name;
	std::string block_type;
	//service extends
	std::string extends_type;
	// serivce
	std::vector<rpc_descriptor> rpcs;
	// struct
	struct_descriptor st;
	// enum
	std::vector<std::string> enum_lines;
};

//class ServiceDescrpitor : public Descriptor
//{};

struct idl_info
{
	std::string file_name;
	std::string absolute_file_path;
	std::string file_name_prefix;
	std::vector<std::string> package_name;
	std::list<Descriptor> desc_list;
	std::list<idl_info> include_list;
	std::list<typedef_descriptor> typedef_list;
	//typedef name -> real cpptype
	std::map<std::string,std::string> typedef_mapping;
};

class SGenUtil
{
public:
	static std::vector<std::string> split_filter_empty(const std::string& str,
													   char sep)
	{
		std::vector<std::string> res;
		std::string::const_iterator start = str.begin();
		std::string::const_iterator end = str.end();
		std::string::const_iterator next = find(start, end, sep);

		while (next != end)
		{
			if (start < next)
				res.emplace_back(start, next);

			start = next + 1;
			next = find(start, end, sep);
		}

		if (start < next)
			res.emplace_back(start, next);

		return res;
	}

	static std::vector<std::string> split_by_space(const std::string& str)
	{
		std::vector<std::string> elems;
		size_t offset = 0;
		size_t start = 0;
		size_t end = 0;
		while(offset < str.size())
		{
			start = SGenUtil::find_next_nonspace(str,offset);
			if (start == std::string::npos)
				break;

			end = SGenUtil::find_next_space(str,start+1);
			if (end == std::string::npos)
			{
				elems.push_back(str.substr(start));
				break;
			}
			elems.push_back(str.substr(start,end-start));
			offset = end+1;
		}
		return elems;
	}

	static std::string strip(const std::string& str)
	{
		std::string res;

		if (!str.empty())
		{
			const char *st = str.c_str();
			const char *ed = st + str.size() - 1;

			while (st <= ed)
			{
				if (!isspace(*st))
					break;

				st++;
			}

			while (ed >= st)
			{
				if (!isspace(*ed))
					break;

				ed--;
			}

			if (ed >= st)
				res.assign(st, ed - st + 1);
		}

		return res;
	}

	static std::string lstrip(const std::string& str)
	{
		std::string res;

		if (!str.empty())
		{
			const char *st = str.c_str();
			const char *ed = st + str.size();

			while (st < ed)
			{
				if (!isspace(*st))
					break;

				st++;
			}

			if (st < ed)
				res.assign(st, ed - st);
		}

		return res;
	}

	static bool start_with(const std::string &str, const std::string prefix)
	{
		size_t prefix_len = prefix.size();

		if (str.size() < prefix_len)
			return false;

		for (size_t i = 0; i < prefix_len; i++)
		{
			if (str[i] != prefix[i])
				return false;
		}

		return true;
	}

	static void replace(std::string& str, const std::string& before,
						const std::string& after)
	{
		for (size_t pos = 0; pos < str.size(); pos += after.length())
		{
			pos = str.find(before, pos);
			if (pos != std::string::npos)
				str.replace(pos, before.length(), after);
			else
				break;
		}
	}

	static size_t find_next_nonspace(const std::string& str, size_t pos)
	{
		for(size_t i = pos; i < str.size(); i++)
		{
			if (!std::isspace(str[i]))
				return i;
		}
		return std::string::npos;
	}

	static size_t find_next_space(const std::string& str, size_t pos)
	{
		for(size_t i = pos; i < str.size(); i++)
		{
			if (std::isspace(str[i]))
				return i;
		}
		return std::string::npos;
	}

	static std::set<std::string> *get_enum_set()
	{
		static std::set<std::string> kInstance;
		return &kInstance;
	}
};

#endif

