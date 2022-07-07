[中文版入口](README_cn.md)

<br/>
<img src="https://raw.githubusercontent.com/wiki/sogou/srpc/srpc-logo-min.png" width = "140" height = "40" alt="srpc-logo"/>

<a href="https://github.com/sogou/srpc/blob/master/LICENSE"><img src="https://img.shields.io/github/license/sogou/srpc?color=379c9c&style=flat-square"/></a>
<a href="https://en.cppreference.com/"><img src="https://img.shields.io/badge/language-C++-black.svg?color=379c9c&style=flat-square"/></a>
<a href="https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-black.svg?color=379c9c&style=flat-square"><img src="https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-black.svg?color=379c9c&style=flat-square"/></a>
<a href="https://github.com/sogou/srpc/releases"><img src="https://img.shields.io/github/v/release/sogou/srpc?color=379c9c&logoColor=ffffff&style=flat-square"/></a>
<a href="https://github.com/sogou/srpc/actions?query=workflow%3A%22ci+build%22++"><img src="https://img.shields.io/github/workflow/status/sogou/srpc/ci%20build?color=379c9c&style=flat-square"/></a>

## Introduction

#### SPRC is an enterprise-level RPC system used by almost all online services in Sogou. It handles tens of billions of requests every day, covering searches, recommendations, advertising system, and other types of services. Its main features include:

* Base on [Sogou C++ Workflow](https://github.com/sogou/workflow), with the following features:
  * High performance, low latency, lightweight
  * Low development and access cost
  * Compatible with SeriesWork and ParallelWork in Workflow
  * One-click migration for existing projects with protobuf/thrift
  * Supports multiple operating systems: Linux / MacOS / Windows
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
  * SRPC
  * BRPC
  * TRPC (the only open-source implementation of TRPC protocol so far)
  * Thrift Framed Binary
  * Thrift Http Binary
* How to use it together with Workflow:
  * You can use the interface to create an RPC task
  * You can put the RPC task into SeriesWork or ParallelWork, and you can also get the current SeriesWork in the callback.
  * You can also use other features supported by Workflow, including upstream, calculation scheduling, asynchronous file IO, etc.
* AOP Modular Plugin Management:
  * Able to report tracing and metrics to [OpenTelemetry](https://opentelemetry.io)
  * Able to pull metrics by [Prometheus](https://prometheus.io)
  * Easy to report to other Cloud Native systems
* srpc Envoy-filter for the users of Kubernetes
* [More features and layers](/docs/en/rpc.md)

## Installation

* srpc will compile the static library libsrpc.a and dynamic library libsrpc.so (or dylib or dll).
* srpc depends on Workflow and protobuf3.
  * For protobuf, you must install protobuf v3.11.0 or above by yourself.
  * For Workflow, it\`s added as dependency automatically via git submodule.
  * For snappy and lz4, source codes are also included as third\_party via git submodule.
  * Workflow, snappy and lz4 can also be found via installed package in the system. If the submodule dependencies are not pulled in thirt_party, they will be searched from the default installation path of the system. The version of snappy is required v1.1.6 or above.
* There is no difference in the srpc code under the Windows version, but users need to use the windows branch of Workflow

~~~sh
git clone --recursive https://github.com/sogou/srpc.git
cd srpc
make
~~~

### Installation(Debian Linux):
srpc has been packaged for Debian. It is currently in Debian sid (unstable) but will eventually be placed into the stable repository.

In order to access the unstable repository, you will need to edit your /etc/apt/sources.list file.

sources.list has the format: `deb <respository server/mirror> <repository name> <sub branches of the repo>`

Simply add the 'unstable' sub branch to your repo:
~~~~sh
deb http://deb.debian.org/ main contrib non-free 

--> 

deb http://deb.debian.org/ unstable main contrib non-free
~~~~

Once that is added, update your repo list and then you should be able to install it:
~~~~sh
sudo apt-get update
~~~~

To install the srpc library for development purposes:
~~~~sh
sudo apt-get install libsrpc-dev
~~~~

To install the srpc library for deployment:
~~~~sh
sudo apt-get install libsrpc
~~~~

## Tutorial

* [Step 1: Design IDL description file](/docs/en/docs-01-idl.md)
* [Step 2: Implement ServiceIMPL](/docs/en/docs-02-service.md)
* [Step 3: Start the Server](/docs/en/docs-03-server.md)
* [Step 4: Use the Client](/docs/en/docs-04-client.md)
* [Step 5: Understand asynchrous Context](/docs/en/docs-05-context.md)
* [Step 6: Use it together with the Workflow](/docs/en/docs-06-workflow.md)
* [Step 7: Common Usage of HTTP](/docs/en/docs-07-http.md)
* [Step 8: Report Tracing to OpenTelemetry](/docs/en/docs-08-tracing.md)
* [Step 9: Report Metrics to OpenTelemetry / Prometheus](/docs/en/docs-09-metrics.md)

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
