[中文版入口](README_cn.md)

<br/>
<img src="https://raw.githubusercontent.com/wiki/sogou/srpc/srpc-logo-min.png" width = "140" height = "40" alt="srpc-logo"/>

<a href="https://github.com/sogou/srpc/blob/master/LICENSE"><img src="https://img.shields.io/github/license/sogou/srpc?color=379c9c&style=flat-square"/></a>
<a href="https://en.cppreference.com/"><img src="https://img.shields.io/badge/language-C++-black.svg?color=379c9c&style=flat-square"/></a>
<a href="https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-black.svg?color=379c9c&style=flat-square"><img src="https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-black.svg?color=379c9c&style=flat-square"/></a>
<a href="https://github.com/sogou/srpc/releases"><img src="https://img.shields.io/github/v/release/sogou/srpc?color=379c9c&logoColor=ffffff&style=flat-square"/></a>
<a href="https://github.com/sogou/srpc/actions?query=workflow%3A%22ci+build%22++"><img src="https://img.shields.io/github/actions/workflow/status/sogou/srpc/ci.yml?branch=master&color=379c9c&style=flat-square"/></a>

### NEW !!!  👉 [SRPC tools : build Workflow and SRPC projects easily.](/tools/README.md)

## Introduction

**SRPC is an enterprise-level RPC system used by almost all online services in Sogou. It handles tens of billions of requests every day, covering searches, recommendations, advertising system, and other types of services.**

Bases on [Sogou C++ Workflow](https://github.com/sogou/workflow), it is an excellent choice for high-performance, low-latency, lightweight RPC systems. Contains AOP aspect-oriented modules that can report Metrics and Trace to a variety of cloud-native systems, such as OpenTelemetry, etc.

Its main features include:

  * Support multiple RPC protocols: [`SPRC`](/tutorial/tutorial-01-srpc_pb_server.cc), [`BRPC`](/tutorial/tutorial-05-brpc_pb_server.cc), [`Thrift`](/tutorial/tutorial-07-thrift_thrift_server.cc), [`TRPC`](/tutorial/tutorial-11-trpc_pb_server.cc) (Currently the only open source implementation of the TRPC protocol)
  * Support multiple operating systems: `Linux`, `MacOS`, `Windows`
  * Support several IDL formats: [`Protobuf`](/tutorial/echo_pb.proto), [`Thrift`](/tutorial/echo_thrift.thrift)
  * Support several data formats transparently: `Json`, `Protobuf`, `Thrift Binary`
  * Support several compression formats, the framework automatically decompresses: `gzip`, `zlib`, `snappy`, `lz4`
  * Support several communication protocols transparently: `tcp`, `http`, `sctp`, `ssl`, `https`
  * With [HTTP+JSON](/docs/docs-07-srpc-http.md), you can communicate with the client or server in any language
  * Use it together with [Workflow Series and Parallel](/docs/docs-06-workflow.md) to facilitate the use of calculations and other asynchronous resources
  * Perfectly compatible with all Workflow functions, such as name service, [upstream](docs/docs-06-workflow.md#3-upstream) and other components
  * Report [Tracing](/docs/docs-08-tracing.md) to [OpenTelemetry](https://opentelemetry.io)
  * Report [Metrics](/docs/docs-09-metrics.md) to [OpenTelemetry](https://opentelemetry.io) and [Prometheus](https://prometheus.io)
  * [More features...](/docs/en/rpc.md)

## Installation

srpc has been packaged for Debian and Fedora. Therefore, we can install it from source code or from the package in the system.

reference: [Linux, MacOS, Windows Installation and Compilation Guide](docs/en/installation.md)

## Quick Start

Let's quickly learn how to use it in a few steps.

For more detailed usage, please refer to [Documents](/docs) and [Tutorial](/tutorial).

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

Terminal 1:

~~~sh
./server
~~~

Terminal 2:

~~~sh
./client
~~~

We can also use CURL to post Http request:

~~~sh
curl 127.0.0.1:8811/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
~~~

Output of Terminal 1:

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

Output of Terminal 2:

~~~sh
message: "Hi, workflow"
~~~

Output of CURL:

~~~sh
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
