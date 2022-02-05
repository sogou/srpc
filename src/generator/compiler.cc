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
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "generator.h"

/* LQ - prototype to determine if the file type is thrift */
enum
{
	TYPE_UNKNOWN = -1,
	TYPE_PROTOBUF = 0,
	TYPE_THRIFT = 1,
};

const char *SRPC_VERSION = "0.9.6";

static int check_idl_type(const char *filename);
const char *SRPC_GENERATOR_USAGE = "\
Usage:\n\t%s [protobuf|thrift] <idl_file> <output_dir>\n";

int main(int argc, const char *argv[])
{
	/* LQ - update to use 2 command line arguments */
	/* TODO: use optarg/getopt_long to have index-independent arguments? */
	/*   Ex: srpc_generator -i <IDL_FILE> -o <OUTPUT_DIR> or */
	/*       srpc_generator --input <IDL_FILE> --output <OUTPUT_DIR> */

	int idl_type;
	int idl_file_id = 1;

	if (argc == 2 &&
		(strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0))
	{
		fprintf(stderr, "srpc_generator version %s\n", SRPC_VERSION);
		return 0;
	}
	else if (argc == 3)
	{
		idl_type = check_idl_type(argv[1]);
		if (idl_type == TYPE_UNKNOWN)
		{
			fprintf(stderr, "ERROR: Invalid IDL file \"%s\"\n", argv[1]);
			fprintf(stderr, SRPC_GENERATOR_USAGE, argv[0]);
			return 0;
		}
	}
	else if (argc == 4)
	{
		if (strcasecmp(argv[1], "protobuf") == 0)
			idl_type = TYPE_PROTOBUF;
		else if (strcasecmp(argv[1], "thrift") == 0)
			idl_type = TYPE_THRIFT;
		else
		{
			fprintf(stderr, "ERROR: Invalid IDL type \"%s\"\n", argv[1]);
			fprintf(stderr, SRPC_GENERATOR_USAGE, argv[0]);
			return 0;
		}

		idl_file_id++;
	}
	else
	{
		fprintf(stderr, SRPC_GENERATOR_USAGE, argv[0]);
		return 0;
	}

	const char *idl_file = argv[idl_file_id];
	struct GeneratorParams params;
	Generator gen(idl_type == TYPE_THRIFT ? true : false);

	params.out_dir = argv[idl_file_id + 1];

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

int check_idl_type(const char *filename)
{
	size_t len = strlen(filename);

	if (len > 6 && strcmp(filename + len - 6, ".proto") == 0)
		return TYPE_PROTOBUF;
	else if (len > 7 && strcmp(filename + len - 7, ".thrift") == 0)
		return TYPE_THRIFT;

	return TYPE_UNKNOWN;
}

