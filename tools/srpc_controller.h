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

#ifndef __SRPC_CONTROLLER__
#define __SRPC_CONTROLLER__

#include "srpc_config.h"

class CommandController
{
public:
	bool parse_args(int argc, const char **argv);
	bool dependencies_and_dir();
	bool copy_template();
	
	virtual void print_usage(const char *name) const = 0;
	virtual bool get_opt(int argc, const char **argv);
	virtual bool check_args();
	virtual bool copy_files() { return true; }

	bool copy_path_files(const char *path);
	virtual bool generate_from_template(const char *file_name, char *format, size_t size) = 0;

public:
	CommandController() { }
	virtual ~CommandController() { }

protected:
	bool get_path(const char *file, char *path, int depth);

protected:
	struct srpc_config config;
};

class HttpController : public CommandController
{
public:
	void print_usage(const char *name) const override;
	bool copy_files() override;
	bool generate_from_template(const char *file_name, char *format, size_t size) override;

public:
	HttpController() { this->config.type = COMMAND_HTTP; };
	~HttpController() { };
};

class RPCController : public CommandController
{
public:
	void print_usage(const char *name) const override;
	bool get_opt(int argc, const char **argv) override;
	bool check_args() override;
	bool copy_files() override;
	bool generate_from_template(const char *file_name, char *format, size_t size) override;

public:
	RPCController() { this->config.type = COMMAND_RPC; };
	~RPCController() { };
};

/*
class DBController : public CommandController
{
public:
	int parse(int argc, const char *argv[]);

public:
	DBController() { };
	~DBController() { };
};

class KafkaController : public CommandController
{
public:
	int parse(int argc, const char *argv[]);

public:
	KafkaController() { };
	~KafkaController() { };
};

class ExtraController : public CommandController
{
public:
	int parse(int argc, const char *argv[]);

public:
	ExtraController() { };
	~ExtraController() { };
};
*/

#endif

