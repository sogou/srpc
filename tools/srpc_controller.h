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
	virtual bool copy_files();
	virtual void print_success_info() const;
	virtual void print_usage(const char *name) const = 0;

protected:
	virtual bool check_args();

private:
	virtual bool get_opt(int argc, const char **argv) = 0;

public:
	CommandController() { }
	virtual ~CommandController() { }

protected:
	using transform_function_t = bool (*)(const std::string&, FILE *,
										  const struct srpc_config *);
	struct file_info
	{
		std::string in_file;
		std::string out_file;
		transform_function_t transform;
	};

	std::vector<struct file_info> default_files;
	struct srpc_config config;

protected:
	bool get_path(const char *file, char *path, int depth);
	bool copy_single_file(const std::string& in_file,
						  const std::string& out_file,
						  transform_function_t transform);
	void fill_rpc_default_files();
};

class HttpController : public CommandController
{
public:
	void print_usage(const char *name) const override;

private:
	bool get_opt(int argc, const char **argv) override;

public:
	HttpController();
	~HttpController() { }
};

class RPCController : public CommandController
{
public:
	void print_usage(const char *name) const override;
	bool copy_files() override;

protected:
	bool check_args() override;

private:
	bool get_opt(int argc, const char **argv) override;

public:
	RPCController();
	~RPCController() { }
};

class ProxyController : public CommandController
{
public:
	void print_usage(const char *name) const override;
	void print_success_info() const override;
	bool copy_files() override;

protected:
	bool check_args() override;

private:
	bool get_opt(int argc, const char **argv) override;

public:
	ProxyController();
	~ProxyController() { }
};

bool common_cmake_transform(const std::string& format, FILE *out,
							const struct srpc_config *config);
bool rpc_idl_transform(const std::string& format, FILE *out,
					   const struct srpc_config *config);
bool rpc_client_transform(const std::string& format, FILE *out,
						  const struct srpc_config *config);
bool rpc_server_transform(const std::string& format, FILE *out,
						  const struct srpc_config *config);
bool rpc_cmake_transform(const std::string& format, FILE *out,
						 const struct srpc_config *config);

#endif

