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

#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "generator.h"

const char *SRPC_VERSION = "0.9.5";

const char *SRPC_GENERATOR_USAGE = "\
Usage: %s [protobuf|thrift] IDL_FILE OUTPUT_DIR\n\
Parse Protobuf IDL or Thrift IDL file and generate output.\n\
Only need to input the top idl_file name.\n\
The correlated files will be parsed recursively.\n\
  -v, --version    Show version info and exit.\n\
  -h, --help       Show this text and exit.\n\
Example :\n\
  %sprotobuf proto_file.proto ./output_dir\n\
  %sthrift thrift_file.thrift ./output_dir\n";

int main(int argc, const char *argv[])
{
	if (argc != 4)
	{
		if (argc == 1 || strcmp(argv[1], "-h") == 0 ||
			strcmp(argv[1], "--help") == 0)
		{
			fprintf(stderr, SRPC_GENERATOR_USAGE, argv[0], argv[0], argv[0]);
		}
		else if (strcmp(argv[1], "-v") == 0 ||
				 strcmp(argv[1], "--version") == 0)
		{
			fprintf(stderr, "srpc_generator version %s\n", SRPC_VERSION);
		}
		else if (strncmp(argv[1], "--", 2) == 0 ||
				 strncmp(argv[1], "-", 1) == 0)
		{
			fprintf(stderr, "Invalid flag. "
					"Please use --help to show usage.\n");
		}
		else if (strcmp(argv[1], "protobuf") && strcmp(argv[1], "thrift"))
		{
			fprintf(stderr, "IDL_TYPE : %s invalid. "
					"Only supports protobuf and thrift.\n", argv[1]);
		}
		else if (argc == 2)
		{
			fprintf(stderr, "Missing IDL_FILE.\n");
		}
		else if (argc == 3)
		{
			fprintf(stderr, "Missing OUTPUT_DIR.\n");
		}
		else
		{
			fprintf(stderr, "Invalid arguments. "
					"Please use --help to show usage.\n");
		}

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

