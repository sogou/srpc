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
#include <string>

#ifdef _WIN32
#include <direct.h>
#define MAXPATHLEN 4096
#else
#include <unistd.h>
#include <sys/param.h>
#include <getopt.h>
#endif

#include "generator.h"

const char *SRPC_VERSION = "0.10.0";

/* LQ - prototype to determine if the file type is thrift */
enum
{
	TYPE_UNKNOWN = -1,
	TYPE_PROTOBUF = 0,
	TYPE_THRIFT = 1,
};

static int check_file_idl_type(const char *filename);
static int parse_origin(int argc, const char *argv[],
						struct GeneratorParams& params,
						int& idl_type);
#ifndef _WIN32
static int parse_getopt(int argc, char * const *argv,
						struct GeneratorParams& params,
						int& idl_type);
#endif
static int parse_prefix_dir(std::string& file, std::string& dir);
static int get_idl_type(const char *argv);
static bool is_root(std::string& file);

const char *SRPC_GENERATOR_USAGE = "\
Usage:\n\
    %s [protobuf|thrift] <idl_file> <output_dir>\n\n\
Available options(linux):\n\
    -f, --idl_file      : IDL file name. If multiple files are imported, specify the top one. Will parse recursively.\n\
    -o, --output_dir    : Output directory.\n\
    -i, --input_dir     : Specify the directory in which to search for imports.\n\
    -s, --skip_skeleton : Skip generating skeleton file. (default: generate)\n\
    -v, --version       : Show version.\n\
    -h, --help          : Show usage.\n";

int main(int argc, const char *argv[])
{
	int idl_type = TYPE_UNKNOWN;
	struct GeneratorParams params;

#ifndef _WIN32
	if (parse_origin(argc, argv, params, idl_type) == 0)
	{
		if (parse_getopt(argc, (char * const *)argv, params, idl_type) != 0)
			return 0;
	}
#else
	parse_origin(argc, argv, params, idl_type);
#endif

	if (params.out_dir == NULL || params.idl_file.empty())
	{
		fprintf(stderr, SRPC_GENERATOR_USAGE, argv[0]);
		return 0;
	}

	if (idl_type == TYPE_UNKNOWN)
		idl_type = check_file_idl_type(params.idl_file.data());

	if (is_root(params.idl_file))
	{
		if (params.input_dir.empty())
			parse_prefix_dir(params.idl_file, params.input_dir);
		else
		{
			fprintf(stderr, "ERROR: input_dir must encompasses the idl_file %s\n",
					params.idl_file.c_str());
			return 0;
		}
	}
	else
	{
		if (params.input_dir.empty() || is_root(params.input_dir) == false)
		{
			char current_dir[MAXPATHLEN] = {};
			getcwd(current_dir, MAXPATHLEN);
			current_dir[strlen(current_dir)] = '/';

			if (params.input_dir.empty())
				params.input_dir = current_dir;
			else
				params.input_dir = current_dir +  params.input_dir;
		}
	}

	Generator gen(idl_type == TYPE_THRIFT ? true : false);

	fprintf(stdout, "[Generator Begin]\n");
	gen.generate(params);
	fprintf(stdout, "[Generator Done]\n");
	return 0;
}

int parse_origin(int argc, const char *argv[],
				 struct GeneratorParams& params, int& idl_type)
{
	int idl_file_id = 1;

	for (int i = 1; i < argc && i < 4 && argv[i][0] != '-'; i++)
	{
		if (i == 1) // parse [protobuf|thrift]
		{
			idl_type = get_idl_type(argv[1]);

			if (idl_type != TYPE_UNKNOWN)
			{
				idl_file_id++;
				continue;
			}
		}

		if (i == idl_file_id) // parse <idl_file>
		{
			int tmp_type = check_file_idl_type(argv[i]);
			if (tmp_type == TYPE_UNKNOWN)
			{
				fprintf(stderr, "ERROR: Invalid IDL file \"%s\".\n", argv[i]);
				return -1;
			}

			if (idl_type == TYPE_UNKNOWN)
				idl_type = tmp_type;

			params.idl_file = argv[idl_file_id];
		}
		else // parse <output_dir>
			params.out_dir = argv[i];
	}

	return 0;
}

#ifndef _WIN32
int parse_getopt(int argc, char * const *argv,
				 struct GeneratorParams& params, int& idl_type)
{
	int ch;

	static struct option longopts[] = {
		{ "version",       no_argument,       NULL, 'v'},
		{ "idl_file",      required_argument, NULL, 'f'},
		{ "output_dir",    required_argument, NULL, 'o'},
		{ "input_dir",     required_argument, NULL, 'i'},
		{ "skip_skeleton", no_argument,       NULL, 's'},
		{ "help",          no_argument,       NULL, 'h'}
	};

	while ((ch = getopt_long(argc, argv, "vf:o:i:sh", longopts, NULL)) != -1)
	{
		switch (ch)
		{
		case 'v':
			fprintf(stderr, "srpc_generator version %s\n", SRPC_VERSION);
			return 1;
		case 'f':
			if (!params.idl_file.empty())
			{
				fprintf(stderr, "Error: -i is duplicate with idl_file %s\n",
						params.idl_file.c_str());
				return -1;
			}
			params.idl_file = optarg;
			break;
		case 'o':
			if (params.out_dir != NULL)
			{
				fprintf(stderr, "Error: -o is duplicate with output_dir %s\n",
						params.out_dir);
				return -1;
			}
			params.out_dir = optarg;
			break;
		case 'i':
			params.input_dir = optarg;
			if (params.input_dir.at(params.input_dir.length() - 1) != '/')
				params.input_dir += '/';
			break;
		case 's':
			params.generate_skeleton = false;
			break;
		case 'h':
			break;
		default:
			return 0;
		}
	}

	return 0;
}
#endif

int get_idl_type(const char *type)
{
	if (strcasecmp(type, "protobuf") == 0)
		return TYPE_PROTOBUF;
	if (strcasecmp(type, "thrift") == 0)
		return TYPE_THRIFT;

	return TYPE_UNKNOWN;
}

// idl file will not start with -
int check_file_idl_type(const char *filename)
{
	size_t len = strlen(filename);

	if (len > 6 && strcmp(filename + len - 6, ".proto") == 0)
		return TYPE_PROTOBUF;
	else if (len > 7 && strcmp(filename + len - 7, ".thrift") == 0)
		return TYPE_THRIFT;

	return TYPE_UNKNOWN;
}

int parse_prefix_dir(std::string& file, std::string& dir)
{
	auto pos = file.find_last_of('/');
	if (pos == std::string::npos)
		return -1;

	dir = file.substr(0, pos + 1);
	file = file.substr(pos + 1);

	return 0;
}

bool is_root(std::string& file)
{
	if (file[0] == '/' || file[1] == ':')
		return true;

	return false;
}

