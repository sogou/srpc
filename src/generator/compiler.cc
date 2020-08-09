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

  Authors: Li Yingxin (liyingxin@sogou-inc.com)
           Wu Jiaxu (wujiaxu@sogou-inc.com)
*/

#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "generator.h"

int main(int argc, const char *argv[])
{
	if (argc != 4)
	{
		fprintf(stderr, "Usage:\n\t%s protobuf proto_file output_dir\n"
				"\t%s thrift thrift_file output_dir\n", argv[0], argv[0]);
		return 0;
	}

	char *idl = const_cast<char *>(argv[1]);
	bool is_thrift = (strcasecmp(idl, "thrift") == 0);
	const char *idl_file = argv[2];
	struct GeneratorParams params;
	Generator gen(is_thrift);
//	Generate(file, parameter, generator_context, error);

	params.out_dir = argv[3];

	char file_name[DIRECTORY_LENGTH] = { 0 };
	size_t pos = 0;

	if (idl_file[0] != '/' && idl_file[1] != ':')
	{
		getcwd(file_name, DIRECTORY_LENGTH);
		pos = strlen(file_name) + 1;
		file_name[strlen(file_name)] = '/';
	}

	if (strlen(idl_file) >= 2 && idl_file[0] == '.' && idl_file[1] == '/')
		idl_file += 2;

	snprintf(file_name + pos, DIRECTORY_LENGTH - pos, "%s", idl_file);

	fprintf(stdout, "[Generator Begin]\n");
	gen.generate(file_name, params);
	fprintf(stdout, "[Generator Done]\n");
	return 0;
}
