[中文版入口](README_cn.md)

# SRPC

[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](https://github.com/sogou/srpc/blob/master/LICENSE)
[![Language](https://img.shields.io/badge/language-c++-red.svg)](https://en.cppreference.com/)
[![Platform](https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-lightgrey.svg)](https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-lightgrey.svg)
[![Build Status](https://github.com/sogou/srpc/actions/workflows/ci.yml/badge.svg)](https://github.com/sogou/srpc/actions/workflows/ci.yml)

## Introduction

#### SRPC is an RPC system developed by Sogou. Its main features include:

* Base on [Sogou C++ Workflow](https://github.com/sogou/workflow), with the following features:
  * High performance
  * Low development and access cost
  * Compatible with SeriesWork and ParallelWork in Workflow
  * One-click migration for existing projects with protobuf/thrift
* Support several IDL formats, including:
  * Protobuf
  * Thrift
* Support several data formats, including:
  * Protobuf serialize
  * Thrift Binary serialize
  * JSON serialize
* Support several compression formats transparently, including:
  * gzip
  * zlib
  * snappy
  * lz4
* Support several communication protocols transparently, including:
  * tcp
  * http
  * sctp
  * ssl
  * https
* With HTTP+JSON, you can use any language:
  * As a server, you can accept POST requests with HTTP server developed in any language and parse the HTTP headers.
  * As a client, you can send POST requests with HTTP client developed in any language and add the required HTTP headers.
* Built-in client/server which can seamlessly communicate with a server/client in other RPC frameworks, including:
  * BRPC
  * TRPC (the only open-source implementation of TRPC protocol so far)
  * ~~GRPC~~
  * Thrift Framed Binary
  * Thrift Http Binary
* How to use it together with Workflow:
  * You can use the interface to create an RPC task
  * You can put the RPC task into SeriesWork or ParallelWork, and you can also get the current SeriesWork in the callback.
  * You can also use other features supported by Workflow, including upstream, calculation scheduling, asynchronous file IO, etc.
* [More features and layers](/docs/en/rpc.md)

## Installation

* srpc is a static library, libsrpc.a. You only need to add the libsrpc as a dependency in the development environment, and it is not required in the compiled binary release.
* srpc depends on Workflow and protobuf3.
  * For protobuf, you must install protobuf v3.0.0 or above by yourself.
  * For Workflow, it\`s added as dependency automatically via git submodule.
  * For snappy and lz4, source codes are also included as third\_party via git submodule.

~~~sh
git clone --recursive https://github.com/sogou/srpc.git
cd srpc
make
sudo make install
~~~

## Tutorial

* [Step 1: Design IDL description file](/docs/en/tutorial-01-idl.md)
* [Step 2: Implement ServiceIMPL](/docs/en/tutorial-02-service.md)
* [Step 3: Start the Server](/docs/en/tutorial-03-server.md)
* [Step 4: Use the Client](/docs/en/tutorial-04-client.md)
* [Step 5: Understand asynchrous Context](/docs/en/tutorial-05-context.md)
* [Step 6: Use it together with the Workflow](/docs/en/tutorial-06-workflow.md)

Easy to compile tutorial with these commands:

~~~sh
cd tutorial
make
~~~

## Quick Start

#### 1\. example.proto

~~~proto
syntax = "proto3";// You can use either proto2 or proto3. Both are supported by srpc

message EchoRequest {
    string message = 1;
    string name = 2;
};

message EchoResponse {
    string message = 1;
};

service Example {
    rpc Echo(EchoRequest) returns (EchoResponse);
};
~~~

#### 2\. generate code

~~~sh
protoc example.proto --cpp_out=./ --proto_path=./
srpc_generator protobuf ./example.proto ./
~~~

#### 3\. server.cc

~~~cpp
#include <stdio.h>
#include <signal.h>
#include "example.srpc.h"

using namespace srpc;

class ExampleServiceImpl : public Example::Service
{
public:
    void Echo(EchoRequest *request, EchoResponse *response, RPCContext *ctx) override
    {
        response->set_message("Hi, " + request->name());

        // gzip/zlib/snappy/lz4/none
        // ctx->set_compress_type(RPCCompressGzip);

        // protobuf/json
        // ctx->set_data_type(RPCDataJson);

        printf("get_req:\n%s\nset_resp:\n%s\n",
                request->DebugString().c_str(), response->DebugString().c_str());
    }
};

void sig_handler(int signo) { }

int main()
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    SRPCServer server_tcp;
    SRPCHttpServer server_http;

    ExampleServiceImpl impl;
    server_tcp.add_service(&impl);
    server_http.add_service(&impl);

    server_tcp.start(1412);
    server_http.start(8811);
    getchar(); // press "Enter" to end.
    server_http.stop();
    server_tcp.stop();

    return 0;
}
~~~

#### 4\. client.cc

~~~cpp
#include <stdio.h>
#include "example.srpc.h"

using namespace srpc;

int main()
{
    Example::SRPCClient client("127.0.0.1", 1412);
    EchoRequest req;
    req.set_message("Hello, srpc!");
    req.set_name("workflow");

    client.Echo(&req, [](EchoResponse *response, RPCContext *ctx) {
        if (ctx->success())
            printf("%s\n", response->DebugString().c_str());
        else
            printf("status[%d] error[%d] errmsg:%s\n",
                    ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
    });

    getchar(); // press "Enter" to end.
    return 0;
}
~~~

#### 5\. make

These compile commands are only for Linux system. On other system, complete cmake in [tutorial](/tutorial) is recommanded.

~~~sh
g++ -o server server.cc example.pb.cc -std=c++11 -lsrpc
g++ -o client client.cc example.pb.cc -std=c++11 -lsrpc
~~~

#### 6\. run

Terminal 1

~~~sh
./server
~~~

Terminal 2

~~~sh
./client
curl 127.0.0.1:8811/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
~~~

Output of Terminal 1

~~~sh
get_req:
message: "Hello, srpc!"
name: "workflow"

set_resp:
message: "Hi, workflow"

get_req:
message: "from curl"
name: "CURL"

set_resp:
message: "Hi, CURL"

~~~

Output of Terminal 2

~~~sh
message: "Hi, workflow"
{"message":"Hi, CURL"}
~~~

## Benchmark

* CPU 2-chip/8-core/32-processor Intel(R) Xeon(R) CPU E5-2630 v3 @2.40GHz
* Memory all 128G
* 10 Gigabit Ethernet
* BAIDU brpc-client in pooled (connection pool) mode

#### QPS at cross-machine single client→ single server under different concurrency

~~~
Client = 1
ClientThread = 64, 128, 256, 512, 1024
RequestSize = 32
Duration = 20s
Server = 1
ServerIOThread = 16
ServerHandlerThread = 16
~~~

![IMG](/docs/images/benchmark2.png)

#### QPS at cross-machine multi-client→ single server under different client processes

~~~
Client = 1, 2, 4, 8, 16
ClientThread = 32
RequestSize = 32
Duration = 20s
Server = 1
ServerIOThread = 16
ServerHandlerThread = 16
~~~

![IMG](/docs/images/benchmark4.png)

#### QPS at same-machine single client→ single server under different concurrency

~~~
Client = 1
ClientThread = 1, 2, 4, 8, 16, 32, 64, 128, 256
RequestSize = 1024
Duration = 20s
Server = 1
ServerIOThread = 16
ServerHandlerThread = 16
~~~

![IMG](/docs/images/benchmark3.png)

#### QPS at same-machine single client→ single server under different request sizes

~~~
Client = 1
ClientThread = 100
RequestSize = 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768
Duration = 20s
Server = 1
ServerIOThread = 16
ServerHandlerThread = 16
~~~

![IMG](/docs/images/benchmark1.png)

#### Latency CDF for fixed QPS at same-machine single client→ single server 

~~~
Client = 1
ClientThread = 50
ClientQPS = 10000
RequestSize = 1024
Duration = 20s
Server = 1
ServerIOThread = 16
ServerHandlerThread = 16
Outiler = 1%
~~~

![IMG](/docs/images/benchmark5.png)

#### Latency CDF for fixed QPS at cross-machine multi-client→ single server 

~~~
Client = 32
ClientThread = 16
ClientQPS = 2500
RequestSize = 512
Duration = 20s
Server = 1
ServerIOThread = 16
ServerHandlerThread = 16
Outiler = 1%
~~~

![IMG](/docs/images/benchmark6.png)

## Contact

* **Email** - **[liyingxin@sogou-inc.com](mailto:liyingxin@sogou-inc.com)** - main author
* **Issue** - You are very welcome to post questions to [issues](https://github.com/sogou/srpc/issues) list.
* **QQ** - Group number: ``618773193``
