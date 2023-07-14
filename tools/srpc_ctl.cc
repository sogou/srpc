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
	printf(COLOR_PINK"Description:\n"
		   COLOR_PURPLE"    Simple generator for building Workflow and SRPC projects.\n\n"
		   COLOR_PINK"Usage:\n"
		   COLOR_INFO"    %s" COLOR_COMMAND " <COMMAND>"
		   COLOR_INFO" <PROJECT_NAME>" COLOR_FLAG " [FLAGS]\n\n"
		   COLOR_PINK"Available Commands:\n"
		   COLOR_COMMAND"    http"
		   COLOR_WHITE"    - create project with both client and server\n"
		   COLOR_COMMAND"    redis"
		   COLOR_WHITE"   - create project with both client and server\n"
		   COLOR_COMMAND"    rpc"
		   COLOR_WHITE"     - create project with both client and server\n"
		   COLOR_COMMAND"    api"
		   COLOR_WHITE"     - create protobuf or thrift IDL api\n"
		   COLOR_COMMAND"    proxy"
		   COLOR_WHITE"   - create proxy for some client and server protocol\n"
		   COLOR_COMMAND"    file"
		   COLOR_WHITE"    - create project with asynchronous file service\n"
		   COLOR_COMMAND"    compute"
		   COLOR_WHITE" - create project with asynchronous computing service\n"
		   COLOR_OFF, name);
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
	else if (strcasecmp(argv[1], "redis") == 0)
	{
		ctl = new RedisController;
	}
	else if (strcasecmp(argv[1], "rpc") == 0)
	{
		ctl = new RPCController;
	}
	else if (strcasecmp(argv[1], "api") == 0)
	{
		ctl = new APIController;
	}
	else if (strcasecmp(argv[1], "proxy") == 0)
	{
		ctl = new ProxyController;
	}
	else if (strcasecmp(argv[1], "file") == 0)
	{
		ctl = new FileServiceController;
	}
	else if (strcasecmp(argv[1], "compute") == 0)
	{
		ctl = new ComputeController;
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
