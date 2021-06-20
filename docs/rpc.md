[English version](/docs/en/rpc.md)

## 基础功能对比
|RPC                        |IDL        |通信  | 网络数据     |压缩                | Attachement |  半同步  |  异步  |  Streaming  |
|---------------------------|-----------|------|------------|--------------------|------------|---------|--------|-------------|
|Thrift Binary Framed       | Thrift    | tcp  | 二进制      |不支持               | 不支持      |  支持    | 不支持  |  不支持      |
|Thrift Binary HttpTransport| Thrift    | http | 二进制      |不支持               | 不支持      |  支持    | 不支持  |  不支持      |
|GRPC                       | PB        | http2| 二进制      |gzip/zlib/lz4/snappy| 支持        |  不支持  | 支持    |  支持       |
|BRPC Std                   | PB        | tcp  | 二进制      |gzip/zlib/lz4/snappy| 支持        |  不支持  | 支持    |  支持       |
|SogouRPC Std               | PB/Thrift | tcp  | 二进制/JSON |gzip/zlib/lz4/snappy| 支持        |  支持    | 支持    |  不支持     |
|SogouRPC Std Http          | PB/Thrift | http | 二进制/JSON |gzip/zlib/lz4/snappy| 支持        |  支持    | 支持    |  不支持     |
|tRPC Std                   | PB        | tcp  | 二进制/JSON |gzip/zlib/lz4/snappy| 支持        |  支持    | 支持    |  不支持     |

## 基础概念
- 通信层：TCP/TPC_SSL/HTTP/HTTPS/HTTP2
- 协议层：Thrift-binary/BRPC-std/SogouRPC-std/tRPC-std
- 压缩层：不压缩/gzip/zlib/lz4/snappy
- 数据层：PB binary/Thrift binary/Json string
- IDL序列化层：PB/Thrift serialization
- RPC调用层：Service/Client IMPL

## RPC Global
- 获取srpc版本号``srpc::SRPCGlobal::get_instance()->get_srpc_version()``

## RPC Status Code
|name                               | value     |含义               |
|-----------------------------------|-----------|-------------------|
|RPCStatusUndefined                 | 0         | 未定义             |
|RPCStatusOK                        | 1         | 正确/成功          |
|RPCStatusServiceNotFound           | 2         | 找不到Service名    |
|RPCStatusMethodNotFound            | 3         | 找不到RPC函数名    |
|RPCStatusMetaError                 | 4         | Meta错误/解析失败  |
|RPCStatusReqCompressSizeInvalid    | 5         | 请求压缩大小错误   |
|RPCStatusReqDecompressSizeInvalid  | 6         | 请求解压大小错误   |
|RPCStatusReqCompressNotSupported   | 7         | 请求压缩类型不支持 |
|RPCStatusReqDecompressNotSupported | 8         | 请求解压类型不支持 |
|RPCStatusReqCompressError          | 9         | 请求压缩失败       |
|RPCStatusReqDecompressError        | 10        | 请求解压失败       |
|RPCStatusReqSerializeError         | 11        | 请求IDL序列化失败  |
|RPCStatusReqDeserializeError       | 12        | 请求IDL反序列化失败|
|RPCStatusRespCompressSizeInvalid   | 13        | 回复压缩大小错误   |
|RPCStatusRespDecompressSizeInvalid | 14        | 回复解压大小错误   |
|RPCStatusRespCompressNotSupported  | 15        | 回复压缩类型不支持 |
|RPCStatusRespDecompressNotSupported| 16        | 回复解压类型不支持 |
|RPCStatusRespCompressError         | 17        | 回复压缩失败       |
|RPCStatusRespDecompressError       | 18        | 回复解压失败       |
|RPCStatusRespSerializeError        | 19        | 回复IDL序列化失败  |
|RPCStatusRespDeserializeError      | 20        | 回复IDL反序列化失败|
|RPCStatusIDLSerializeNotSupported  | 21        | 不支持IDL序列化   |
|RPCStatusIDLDeserializeNotSupported| 22        | 不支持IDL反序列化 |
|RPCStatusURIInvalid                | 30        | URI非法          |
|RPCStatusUpstreamFailed            | 31        | Upstream全熔断   |
|RPCStatusSystemError               | 100       | 系统错误         |
|RPCStatusSSLError                  | 101       | SSL错误          |
|RPCStatusDNSError                  | 102       | DNS错误          |
|RPCStatusProcessTerminated         | 103       | 程序退出&终止     |

## RPC IDL
- 描述文件
- 前后兼容
- Protobuf/Thrift

### 示例
下面我们通过一个具体例子来呈现
- 我们拿pb举例，定义一个ServiceName为``Example``的``example.proto``文件
- rpc接口名为``Echo``，输入参数为``EchoRequest``，输出参数为``EchoResponse``
- ``EchoRequest``包括两个string：``message``和``name``
- ``EchoResponse``包括一个string：``message``

~~~proto
syntax="proto2";

message EchoRequest {
    optional string message = 1;
    optional string name = 2;
};

message EchoResponse {
    optional string message = 1;
};

service Example {
     rpc Echo(EchoRequest) returns (EchoResponse);
};
~~~

## RPC Service
- 组成sogouRPC服务的基本单元
- 每一个Service一定由某一种IDL生成
- Service由IDL决定，与网络通信具体协议无关

### 示例
下面我们通过一个具体例子来呈现
- 沿用上面的``example.proto``IDL描述文件
- 执行官方的``protoc example.proto --cpp_out=./ --proto_path=./``获得``example.pb.h``和``example.pb.cpp``两个文件
- 执行SogouRPC的``srpc_generator protobuf ./example.proto ./``获得``example.srpc.h``文件
- 我们派生``Example::Service``来实现具体的rpc业务逻辑，这就是一个RPC Service
- 注意这个Service没有任何网络、端口、通信协议等概念，仅仅负责完成实现从``EchoRequest``输入到输出``EchoResponse``的业务逻辑

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
    void Echo(EchoRequest *request, EchoResponse *response, RPCContext *ctx) override
    {
        response->set_message("Hi, " + request->name());

        printf("get_req:\n%s\nset_resp:\n%s\n",
                request->DebugString().c_str(),
                response->DebugString().c_str());
    }
};
~~~

## RPC Server
- 每一个Server对应一个端口
- 每一个Server对应一个确定的网络通信协议
- 一个Service可以添加到任意的Server里
- 一个Server可以拥有任意多个Service，但在当前Server里ServiceName必须唯一
- 不同IDL的Service是可以放进同一个Server中的

### 示例
下面我们通过一个具体例子来呈现
- 沿用上面的``ExampleServiceImpl``Service
- 首先，我们创建1个RPC Server，并确定proto文件的内容
- 然后，我们可以创建任意个数的Service实例、任意不同IDL proto形成的Service，把这些Service通过``add_service()``接口添加到Server里
- 最后，通过Server的``start()``或者``serve()``开启服务，处理即将到来的rpc请求

- 想像一下，我们也可以从``Example::Service``派生出多个Service，而它们的rpc``Echo``实现的功能可以不同
- 想像一下，我们可以在N个不同的端口创建N个不同的RPC Server，代表着不同的协议
- 想像一下，我们可以把同一个ServiceIMPL实例``add_service()``到不同的Server上，我们也可以把不同的ServiceIMPL实例``add_service``到同一个Server上
- 想像一下，我们可以用同一个``ExampleServiceImpl``，在三个不同端口、同时服务于BPRC-STD、SogouRPC-STD、SogouRPC-Http
- 甚至，我们可以将1个Protobuf IDL相关的``ExampleServiceImpl``和1个Thrift IDL相关的``AnotherThriftServiceImpl``，``add_service``到同一个SogouRPC-STD Server，两种IDL在同一个端口上完美工作！

~~~cpp
int main()
{
    SRPCServer server_srpc;
    SRPCHttpServer server_srpc_http;
    BRPCServer server_brpc;
    ThriftServer server_thrift;

    ExampleServiceImpl impl_pb;
    AnotherThriftServiceImpl impl_thrift;

    server_srpc.add_service(&impl_pb);
    server_srpc.add_service(&impl_thrift);
    server_srpc_http.add_service(&impl_pb);
    server_srpc_http.add_service(&impl_thrift);
    server_brpc.add_service(&impl_pb);
    server_thrift.add_service(&impl_thrift);

    server_srpc.start(1412);
    server_srpc_http.start(8811);
    server_brpc.start(2020);
    server_thrift.start(9090);
    pause();
    server_thrift.stop();
    server_brpc.stop();
    server_srpc_http.stop();
    server_srpc.stop();

    return 0;
}
~~~

## RPC Client
- 每一个Client对应着一个确定的目标/一个确定的集群
- 每一个Client对应着一个确定的网络通信协议
- 每一个Client对应着一个确定的IDL

### 示例
下面我们通过一个具体例子来呈现
- 沿用上面的例子，client相对简单，直接调用即可
- 通过``Example::XXXClient``创建某种RPC的client实例，需要目标的ip+port或url
- 利用client实例直接调用rpc函数``Echo``即可，这是一次异步请求，请求完成后会进入回调函数
- 具体的RPC Context用法请看下一个段落: [RPC Context](/docs/rpc.md#rpc-context))

~~~cpp
#include <stdio.h>
#include "example.srpc.h"

using namespace srpc;

int main()
{
    Example::SRPCClient client("127.0.0.1", 1412);
    EchoRequest req;
    req.set_message("Hello, sogou rpc!");
    req.set_name("Li Yingxin");

    client.Echo(&req, [](EchoResponse *response, RPCContext *ctx) {
        if (ctx->success())
            printf("%s\n", response->DebugString().c_str());
        else
            printf("status[%d] error[%d] errmsg:%s\n",
                    ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
    });

    pause();
    return 0;
}
~~~

## RPC Context
- RPCContext专门用来辅助异步接口，Service和Client通用
- 每一个异步接口都会提供Context，用来给用户提供更高级的功能，比如获取对方ip、获取连接seqid等
- Context上一些功能是Server或Client独有的，比如Server可以设置回复数据的压缩方式，Client可以获取请求成功或失败
- Context上可以通过g``et_series()``获得所在的series，与workflow的异步模式无缝结合

### RPCContext API - Common
#### ``long long get_seqid() const;``
请求+回复视为1次完整通信，获得当前socket连接上的通信sequence id，seqid=0代表第1次

#### ``std::string get_remote_ip() const;``
获得对方IP地址，支持ipv4/ipv6

#### ``int get_peer_addr(struct sockaddr *addr, socklen_t *addrlen) const;``
获得对方地址，in/out参数为更底层的数据结构sockaddr

#### ``const std::string& get_service_name() const;``
获取RPC Service Name

#### ``const std::string& get_method_name() const;``
获取RPC Methode Name

#### ``SeriesWork *get_series() const;``
获取当前ServerTask/ClientTask所在series

### RPCContext API - Only for client done
#### ``bool success() const;``
client专用。这次请求是否成功

#### ``int get_status_code() const;``
client专用。这次请求的rpc status code

#### ``const char *get_errmsg() const;``
client专用。这次请求的错误信息

#### ``int get_error() const;``
client专用。这次请求的错误码

#### ``void *get_user_data() const;``
client专用。获取ClientTask的user_data。如果用户通过``create_xxx_task()``接口产生task，则可以通过user_data域记录上下文，在创建task时设置，在回调函数中拿回。

### RPCContext API - Only for server process
#### ``void set_data_type(RPCDataType type);``
Server专用。设置数据打包类型
- RPCDataProtobuf
- RPCDataThrift
- RPCDataJson

#### ``void set_compress_type(RPCCompressType type);``
Server专用。设置数据压缩类型(注：Client的压缩类型在Client或Task上设置)
- RPCCompressNone
- RPCCompressSnappy
- RPCCompressGzip
- RPCCompressZlib
- RPCCompressLz4

#### ``void set_attachment_nocopy(const char *attachment, size_t len);``
Server专用。设置attachment附件。

#### ``bool get_attachment(const char **attachment, size_t *len) const;``
Server专用。获取attachment附件。

#### ``void set_reply_callback(std::function<void (RPCContext *ctx)> cb);``
Server专用。设置reply callback，操作系统写入socket缓冲区成功后被调用。

#### ``void set_send_timeout(int timeout);``
Server专用。设置发送超时，单位毫秒。-1代表无限。

#### ``void set_keep_alive(int timeout);``
Server专用。设置连接保活时间，单位毫秒。-1代表无限。

## RPC Options
### Server Params
|name                       |默认                      |含义                             |
|---------------------------|--------------------------|--------------------------------|
|max_connections            | 2000                     | Server的最大连接数，默认2000个   |
|peer_response_timeout      | 10 * 1000                | 每一次IO的读超时，默认10秒       |
|receive_timeout            | -1                       | 每一条完整消息的读超时，默认无限  |
|keep_alive_timeout         | 60 * 1000                | 空闲连接保活，-1代表永远不断开，0代表短连接，默认长连接保活60秒 |
|request_size_limit         | 2LL * 1024 * 1024 * 1024 | 请求包大小限制，最大2GB           |
|ssl_accept_timeout         | 10 * 1000                | SSL连接超时，默认10秒            |

### Client Params
|name                       |默认                      |含义                             |
|---------------------------|--------------------------|--------------------------------|
|host                       | ""                       | 目标host，可以是ip、域名         |
|port                       | 1412                     | 目标端口号，默认1412            |
|is_ssl                     | false                    | ssl开关，默认关闭               |
|url                        | ""                       | 当host为空，url设置才有效。url将屏蔽host/port/is_ssl三项 |
|task_params                | TASK默认配置              | 见下方                         |

### Task Params
|name                       |默认                      |含义                             |
|---------------------------|--------------------------|--------------------------------|
|send_timeout               | -1                       | 发送写超时，默认无限             |
|receive_timeout            | -1                       | 回复超时，默认无限             |
|watch_timeout              | 0                        | 对方第一次回复的超时，默认0不设置 |
|keep_alive_timeout         | 30 * 1000                | 空闲连接保活，-1代表永远不断开，默认30s |
|retry_max                  | 0                        | 最大重试次数，默认0不重试        |
|compress_type              | RPCCompressNone          | 压缩类型，默认不压缩             |
|data_type                  | RPCDataUndefined         | 网络包数据类型，默认与RPC默认值一致，SRPC-Http协议为json，其余为对应IDL的类型 |

## 与workflow异步框架的结合
### Server
下面我们通过一个具体例子来呈现
- Echo RPC在接收到请求时，向下游发起一次http请求
- 对下游请求完成后，我们将http response的body信息填充到response的message里，回复给客户端
- 我们不希望阻塞/占据着Handler的线程，所以对下游的请求一定是一次异步请求
- 首先，我们通过Workflow框架的工厂``WFTaskFactory::create_http_task``创建一个异步任务http_task
- 然后，我们利用RPCContext的``ctx->get_series()``获取到ServerTask所在的SeriesWork
- 最后，我们使用SeriesWork的``push_back``接口将http_task放到SeriesWork的后面

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
    void Echo(EchoRequest *request, EchoResponse *response, RPCContext *ctx) override
    {
        auto *http_task = WFTaskFactory::create_http_task("https://www.sogou.com", 0, 0,
            [request, response](WFHttpTask *task) {
                if (task->get_state() == WFT_STATE_SUCCESS)
                {
                    const void *data;
                    size_t len;
                    task->get_resp()->get_parsed_body(&data, &len);
                    response->mutable_message()->assign((const char *)data, len);
                }
                else
                    response->set_message("Error: " + std::to_string(task->get_error()));

                printf("Server Echo()\nget_req:\n%s\nset_resp:\n%s\n",
                                            request->DebugString().c_str(),
                                            response->DebugString().c_str());
            });

        ctx->get_series()->push_back(http_task);
    }
};
~~~

### Client
下面我们通过一个具体例子来呈现
- 我们并行发出两个请求，1个是rpc请求，1个是http请求
- 两个请求都结束后，我们再发起一次计算任务，计算两个数的平方和
- 首先，我们通过RPC Client的``create_Echo_task``创建一个rpc异步请求的网络任务rpc_task
- 然后，我们通过Workflow框架的工厂``WFTaskFactory::create_http_task``和``WFTaskFactory::create_go_task``分别创建异步网络任务http_task，和异步计算任务calc_task
- 最后，我们利用串并联流程图，乘号代表并行、大于号代表串行，将3个异步任务组合起来执行``start()``

~~~cpp
void calc(int x, int y)
{
    int z = x * x + y * y;

    printf("calc result: %d\n", z);
}

int main()
{
    Example::SRPCClient client("127.0.0.1", 1412);

    auto *rpc_task = client.create_Echo_task([](EchoResponse *response, RPCContext *ctx) {
        if (ctx->success())
            printf("%s\n", response->DebugString().c_str());
        else
            printf("status[%d] error[%d] errmsg:%s\n",
                    ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
    });

    auto *http_task = WFTaskFactory::create_http_task("https://www.sogou.com", 0, 0, [](WFHttpTask *task) {
        if (task->get_state() == WFT_STATE_SUCCESS)
        {
            std::string body;
            const void *data;
            size_t len;

            task->get_resp()->get_parsed_body(&data, &len);
            body.assign((const char *)data, len);
            printf("%s\n\n", body.c_str());
        }
        else
            printf("Http request fail\n\n");
    });

    auto *calc_task = WFTaskFactory::create_go_task(calc, 3, 4);

    EchoRequest req;
    req.set_message("Hello, sogou rpc!");
    req.set_name("1412");
    rpc_task->serialize_input(&req);

    ((*http_task * rpc_task) > calc_task).start();

    pause();
    return 0;
}
~~~
