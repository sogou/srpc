#include <string>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <string>

#include "echo.srpc.h"
#include "msg.srpc.h"

using namespace srpc;
using namespace example;

class ExamplePBServiceImpl : public ExamplePB::Service
{
public:
	void Echo(EchoRequest *request, EchoResponse *response, RPCContext *ctx) override
	{
/*
		ctx->set_compress_type(RPCCompressGzip);
		char buf[100000];
		for (int i = 0; i < 100000; i++)
			buf[i] = 'a';
		buf[99999] = '\0';
		response->set_message(buf);
*/
		response->set_message("Hi back");

//		printf("Server Echo()\nget_req:\n%s\nset_resp:\n%s\n",
//									request->DebugString().c_str(),
//									response->DebugString().c_str());
	}
};

class ExampleThriftServiceImpl : public ExampleThrift::Service
{
/*
public:
	void Message(MessageResult& _return, const std::string& message, const std::string& name)
	{
		_return.message = "Hi back";
	}*/

public:
	void Message(ExampleThrift::MessageRequest *request, ExampleThrift::MessageResponse *response, RPCContext *ctx) override
	{
		//ctx->set_data_type(RPCDataJson);

		ctx->set_compress_type(RPCCompressGzip);
		char buf[10000];
		for (int i = 0; i < 10000; i++)
			buf[i] = 'a';
		buf[9999] = '\0';
		response->result.message = buf;

//		response->result.message = "Hi back";
		//response->result.__isset.arr = true;
		//response->result.state = 1;
		//response->result.__isset.state = true;
		//response->result.error = 0;
		//response->result.__isset.error = true;

//		printf("Server Echo()\nget_req:\n%s\nset_resp:\n%s\n",
//									request->message.c_str(),
//									response->result.message.c_str());
	}
};

static void sig_handler(int signo)
{
}

template<class SERVER>
static void run_server(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;
	RPCSpanLoggerDefault RPC_SPAN_LOGGER_DEFAULT;
	RPC_SPAN_LOGGER_DEFAULT.set_span_limit(2);

	params.span_logger = &RPC_SPAN_LOGGER_DEFAULT;
	SERVER server(&params);

	ExamplePBServiceImpl pb_impl;
	ExampleThriftServiceImpl thrift_impl;
	server.add_service(&pb_impl);
	server.add_service(&thrift_impl);

	// some server options
	if (server.start(port) == 0)
	{
		pause();
		server.stop();
	}
	else
		perror("server start");
}

template<class SERVER>
static void run_thrift_server(unsigned short port)
{
	RPCServerParams params = RPC_SERVER_PARAMS_DEFAULT;
	params.max_connections = 2048;

	SERVER server(&params);

	ExampleThriftServiceImpl thrift_impl;
	server.add_service(&thrift_impl);

	// some server options
	if (server.start(port) == 0)
	{
		pause();
		server.stop();
	}
	else
		perror("server start");
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <PORT> <srpc|brpc|thrift>\n", argv[0]);
		return 0;
	}

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	WFGlobalSettings my = GLOBAL_SETTINGS_DEFAULT;
	my.poller_threads = 16;
	my.handler_threads = 16;
	WORKFLOW_library_init(&my);

	unsigned short port = atoi(argv[1]);
	std::string server_type = argv[2];

	if (server_type == "srpc")
		run_server<SRPCServer>(port);
	else if (server_type == "brpc")
		run_server<BRPCServer>(port);
	else if (server_type == "thrift")
		run_thrift_server<ThriftServer>(port);
	else if (server_type == "srpc_http")
		run_server<SRPCHttpServer>(port);
	else if (server_type == "thrift_http")
		run_thrift_server<ThriftHttpServer>(port);
	else
		abort();

	google::protobuf::ShutdownProtobufLibrary();	
	return 0;
}

