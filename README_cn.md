[English version](README.md) &nbsp;&nbsp;|&nbsp;&nbsp; [Wiki：SRPC架构介绍](/docs/wiki.md)

<img src="https://raw.githubusercontent.com/wiki/sogou/srpc/srpc-logo-min.png" width = "140" height = "40" alt="srpc-logo"/>

<a href="https://github.com/sogou/srpc/blob/master/LICENSE"><img src="https://img.shields.io/github/license/sogou/srpc?color=379c9c&style=flat-square"/></a>
<a href="https://en.cppreference.com/"><img src="https://img.shields.io/badge/language-C++-black.svg?color=379c9c&style=flat-square"/></a>
<a href="https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-black.svg?color=379c9c&style=flat-square"><img src="https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-black.svg?color=379c9c&style=flat-square"/></a>
<a href="https://github.com/sogou/srpc/releases"><img src="https://img.shields.io/github/v/release/sogou/srpc?color=379c9c&logoColor=ffffff&style=flat-square"/></a>
<a href="https://github.com/sogou/srpc/actions?query=workflow%3A%22ci+build%22++"><img src="https://img.shields.io/github/actions/workflow/status/sogou/srpc/ci.yml?branch=master&color=379c9c&style=flat-square"/></a>

### NEW !!!  👉 [SRPC tools : 一个帮你快速构建Workflow和SRPC项目的小工具.](/tools/README_cn.md) 

## Introduction
#### SRPC是全搜狗业务线上使用的企业级RPC系统，目前每天承载上百亿的请求量，涵盖搜广推及其他类型业务。主要功能和特点：
  * 底层基于[Sogou C++ Workflow](https://github.com/sogou/workflow)，兼具：
    * 高性能、低延迟、轻量级
    * 低开发和接入门槛
    * 完美兼容workflow的串并联任务流
    * 对于已有protobuf/thrift描述文件的项目，可以做到一键迁移
    * 支持Linux / MacOS / Windows等多操作系统
  * 支持多种IDL格式，包括：
    * Protobuf
    * Thrift
  * 支持多种数据布局，使用上完全透明，包括：
    * Protobuf serialize
    * Thrift Binary serialize
    * json serialize
  * 支持多种压缩，使用上完全透明，包括：
    * gzip
    * zlib
    * snappy
    * lz4
  * 支持多种通信协议，使用上完全透明，包括：
    * tcp
    * http
	* sctp
	* ssl
	* https
  * 用户可以通过http+json实现跨语言：
    * 如果自己是server提供方，用任何语言的http server接受post请求，解析若干http header即可
    * 如果自己是client调用方，用任何语言的http client发送post请求，添加若干http header即可
  * 内置了可以与其他RPC框架的server/client无缝互通的client/server，包括：
    * SRPC
    * BRPC
    * TRPC (目前唯一的TRPC协议开源实现)
    * Thrift Framed Binary
    * Thrift Http Binary
  * 兼容workflow的使用方式：
    * 提供创建任务的接口来创建一个rpc任务
    * 可以把rpc任务放到任务流图中，回调函数里也可以拿到当前的任务流
    * workflow所支持的其他功能，包括upstream、计算调度、异步文件IO等
  * AOP模块化插件管理：
    * 可上报tracing和metrics到[OpenTelemetry](https://opentelemetry.io)
    * 可上报监控到[Prometheus](https://prometheus.io)
    * 轻松对接其他云原生系统
  * 支持srpc协议的Envoy-filter，满足Kubernetes用户的使用需求
  * [更多功能和层次介绍](docs/rpc.md)

## Installation
  * srpc默认会编译出静态库libsrpc.a和动态库libsrpc.so(或者dylib或dll)
  * srpc依赖workflow和protobuf3
    * protobuf需要用户自行安装v3.11.0或以上的版本
    * workflow可以通过git的submodule形式进行依赖
    * 压缩库snappy和lz4也以submodule的形式在third_party/中作源码依赖
    * workflow、snappy和lz4也可以系统预装，如果thirt_party中没有拉取源码依赖，则会从系统默认安装路径寻找，snappy的预装要求版本是v1.1.6或以上
  * Windows版下srpc代码无差异，需要依赖workflow的windows分支
~~~sh
git clone --recursive https://github.com/sogou/srpc.git
cd srpc
make
~~~

## Tutorial

* [第1步：设计IDL描述文件](docs/docs-01-idl.md)
* [第2步：实现ServiceIMPL](docs/docs-02-service.md)
* [第3步：启动Server](docs/docs-03-server.md)
* [第4步：使用Client](docs/docs-04-client.md)
* [第5步：了解异步Context](docs/docs-05-context.md)
* [第6步：与workflow的结合使用](docs/docs-06-workflow.md)
* [第7步：使用HTTP协议互通](docs/docs-07-http.md)
* [第8步：链路上报到OpenTelemetry](docs/docs-08-tracing.md)
* [第9步：监控指标的使用与上报](docs/docs-09-metrics.md)

简单的命令即可编译示例：

~~~sh
 cd tutorial
 make
~~~

## Quick Start
#### 1. example.proto
~~~proto
syntax = "proto3";//这里proto2和proto3都可以，srpc都支持

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

#### 2. generate code
~~~sh
protoc example.proto --cpp_out=./ --proto_path=./
srpc_generator protobuf ./example.proto ./
~~~

#### 3. server.cc
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

#### 4. client.cc
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

#### 5. make
在Linux系统下的编译示例如下，其他平台建议到[tutorial](/tutorial)目录下使用完整的cmake文件协助解决编译依赖问题。
~~~sh
g++ -o server server.cc example.pb.cc -std=c++11 -lsrpc
g++ -o client client.cc example.pb.cc -std=c++11 -lsrpc
~~~

#### 6. run
终端1：
~~~sh
./server
~~~
终端2：
~~~sh
./client
~~~
也可以用CURL发送http请求：
~~~sh
curl 127.0.0.1:8811/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
~~~

终端1输出：
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
终端2输出：
~~~sh
message: "Hi, workflow"
~~~
CURL收到的回复：
~~~sh
{"message":"Hi, CURL"}
~~~

## Benchmark

* CPU 2-chip/8-core/32-processor Intel(R) Xeon(R) CPU E5-2630 v3 @2.40GHz
* Memory all 128G
* 10 Gigabit Ethernet
* BAIDU brpc-client使用连接池pooled模式

#### 跨机单client→单server在不同并发的QPS
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

#### 跨机多client→单server在不同client进程数的QPS
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

#### 同机单client→单server在不同并发下的QPS
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

#### 同机单client→单server在不同请求大小下的QPS
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

#### 同机单client→单server在固定QPS下的延时CDF
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

#### 跨机多client→单server在固定QPS下的延时CDF
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

## 与我们联系
 * **Email** - **[liyingxin@sogou-inc.com](mailto:liyingxin@sogou-inc.com)** - 主要作者
 * **Issue** - 使用中的任何问题都欢迎到[issues](https://github.com/sogou/srpc/issues)进行交流。
 * **QQ** - 群号: ``618773193``
