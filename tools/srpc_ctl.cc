/*
  Copyright (c) 2022 Sogou, Inc.

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
#include "srpc_controller.h"

static void usage(const char *name)
{
	printf("Description:\n"
		   "    A handy generator for Workflow and SRPC project.\n\n"
		   "Usage:\n"
		   "    %s <COMMAND> <PROJECT_NAME> [FLAGS]\n\n"
		   "Available Commands:\n"
		   "    \"http\"  - create project with both client and server\n"
		   "    \"rpc\"   - create project with both client and server\n"
		   "    \"redis\" - create redis client\n"
		   "    \"mysql\" - create mysql client\n"
		   "    \"kafka\" - create kafka client\n"
		   "    \"proxy\" - create proxy for some client and server protocol\n"
		   "    \"file\"  - create project with file service\n"
		   , name);
}

int main(int argc, const char *argv[])
{
	if (argc < 2)
	{
		usage(argv[0]);
		return 0;
	}

	CommandController *ctl;

	if (strcasecmp(argv[1], "http") == 0)
	{
		ctl = new HttpController;
	}
	else if (strcasecmp(argv[1], "rpc") == 0)
	{
		ctl = new RPCController;
	}
	else
	{
		usage(argv[0]);
		return 0;
	}

	if (ctl->parse_args(argc, argv) == true)
	{
		if (ctl->dependencies_and_dir() == true)
		{
			if (ctl->copy_files() == true)
				ctl->print_success_info();
		}
	}
	else
		ctl->print_usage(argv[0]);

	delete ctl;
	return 0;
}
