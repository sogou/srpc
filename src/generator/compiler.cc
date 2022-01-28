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
#include <fstream>

/* LQ - prototype to determine if the file type is thrift */
bool isThrift(char* filename);

int main(int argc, const char *argv[])
{
	/* LQ - update to use 2 command line arguments */
	/* TODO: use optarg/getopt_long to have index-independent arguments? */
	/* 	 Ex: srpc_generator -i <IDL_FILE> -o <OUTPUT_DIR> or */
	/*  	     srpc_generator --input <IDL_FILE> --output <OUTPUT_DIR> */
	if(argc != 3) {
		fprintf(stderr, "Usage:\n\t%s <idl_file> <output_dir>\n", argv[0]);
		return 0;
	}
	char *idl = const_cast<char *>(argv[1]);
	bool is_thrift = isThrift(idl);
	const char *idl_file = argv[1];
	struct GeneratorParams params;
	Generator gen(is_thrift);

	params.out_dir = argv[2];

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

/* LQ - function to determine if the input file is thrift or not */
/* Date: 28 JAN 2022 */
/* Description: Opens a file, reads the lines and checks for "syntax = proto", */
/* 		which indicates a protobuf file. If it does not find this string, */
/* 		return true and assume the file is thrift. Error out if file is unable to be read */
/* Notes/TODO:  * Add enum to handle file types? (TYPE_THRIFT, TYPE_PROTOBUF, TYPE_UNKNOWN) */
/*		* Better detection of file types? Better behaviour if thrift files have a specific format to look for */
/*		* Handle error case when user provides a file that is not thrift nor protobuf */
bool isThrift(char* filename) {

	std::ifstream idl_file;
	std::string line;
	idl_file.open(filename);

	if(idl_file.is_open()) {
		while(!idl_file.eof()) {
			getline(idl_file, line);
			/* If the line contains "syntax = proto", it is a protobuf file */
			if(line.find("syntax = proto") != std::string::npos) {
				idl_file.close();
				return false;
			}	
		}
		idl_file.close();
	} else {
		/* Error case */
		fprintf(stderr, "Unable to open IDL file: %s\n", filename);
		exit(0);
	}
	/* Otherwise, return true (assume thrift format) */
	return true;
}
