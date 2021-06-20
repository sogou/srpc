[中文版](/docs/rpc.md)

## Comparison of basic features

| RPC                         | IDL       | Communication | Network data | Compression          | Attachment    | Semi-synchronous | Asynchronous  | Streaming     |
| --------------------------- | --------- | ------------- | ------------ | -------------------- | ------------- | ---------------- | ------------- | ------------- |
| Thrift Binary Framed        | Thrift    | TCP           | Binary       | Not supported        | Not supported | Supported        | Not supported | Not supported |
| Thrift Binary HttpTransport | Thrift    | HTTP          | Binary       | Not supported        | Not supported | Supported        | Not supported | Not supported |
| GRPC                        | PB        | HTTP2         | Binary       | gzip/zlib/lz4/snappy | Supported     | Not supported    | Supported     | Supported     |
| BRPC Std                    | PB        | TCP           | Binary       | gzip/zlib/lz4/snappy | Supported     | Not supported    | Supported     | Supported     |
| SogouRPC Std                | PB/Thrift | TCP           | Binary/JSON  | gzip/zlib/lz4/snappy | Supported     | Supported        | Supported     | Not supported |
| SogouRPC Std HTTP           | PB/Thrift | HTTP          | Binary/JSON  | gzip/zlib/lz4/snappy | Supported     | Supported        | Supported     | Not supported |
| tRPC Std                    | PB        | TCP           | Binary/JSON  | gzip/zlib/lz4/snappy | Supported     | Supported        | Supported     | Not supported |

## Basic concepts

- Communication layer: TCP/TPC\_SSL/HTTP/HTTPS/HTTP2
- Protocol layer: Thrift-binary/BRPC-std/SogouRPC-std/tRPC-std
- Compression layer: no compression/gzip/zlib/lz4/snappy
- Data layer: PB binary/Thrift binary/JSON string
- IDL serialization layer: PB/Thrift serialization
- RPC invocation layer: Service/Client IMPL

## RPC Global

- Get srpc version `srpc::SRPCGlobal::get_instance()->get_srpc_version()`

## RPC Status Code

| Name                                | Value | Description                                              |
| ----------------------------------- | ----- | -------------------------------------------------------- |
| RPCStatusUndefined                  | 0     | Undefined                                                |
| RPCStatusOK                         | 1     | Correct/Success                                          |
| RPCStatusServiceNotFound            | 2     | Cannot find the RPC service name                             |
| RPCStatusMethodNotFound             | 3     | Cannot find the RPC function name                        |
| RPCStatusMetaError                  | 4     | Meta error/ parsing failed                               |
| RPCStatusReqCompressSizeInvalid     | 5     | Incorrect size for the request when compressing          |
| RPCStatusReqDecompressSizeInvalid   | 6     | Incorrect size for the request when decompressing        |
| RPCStatusReqCompressNotSupported    | 7     | The compression type for the request is not supported    |
| RPCStatusReqDecompressNotSupported  | 8     | The decompression type for the request is not supported  |
| RPCStatusReqCompressError           | 9     | Failed to compress the request                           |
| RPCStatusReqDecompressError         | 10    | Failed to decompress the request                         |
| RPCStatusReqSerializeError          | 11    | Failed to serialize the request by IDL                   |
| RPCStatusReqDeserializeError        | 12    | Failed to deserialize the request by IDL                 |
| RPCStatusRespCompressSizeInvalid    | 13    | Incorrect size for the response when compressing         |
| RPCStatusRespDecompressSizeInvalid  | 14    | Incorrect size for the response when compressing         |
| RPCStatusRespCompressNotSupported   | 15    | The compression type for the response is not supported   |
| RPCStatusRespDecompressNotSupported | 16    | The decompression type for the response not supported    |
| RPCStatusRespCompressError          | 17    | Failed to compress the response                          |
| RPCStatusRespDecompressError        | 18    | Failed to decompress the response                        |
| RPCStatusRespSerializeError         | 19    | Failed to serialize the response by IDL                  |
| RPCStatusRespDeserializeError       | 20    | Failed to deserialize the response by IDL                |
| RPCStatusIDLSerializeNotSupported   | 21    | IDL serialization type is not supported                  |
| RPCStatusIDLDeserializeNotSupported | 22    | IDL deserialization type is not supported                |
| RPCStatusURIInvalid                 | 30    | Illegal URI                                              |
| RPCStatusUpstreamFailed             | 31    | Upstream is failed                                       |
| RPCStatusSystemError                | 100   | System error                                             |
| RPCStatusSSLError                   | 101   | SSL error                                                |
| RPCStatusDNSError                   | 102   | DNS error                                                |
| RPCStatusProcessTerminated          | 103   | Program exited&terminated                                |

## RPC IDL

- Interface Description Languaue file
- Backward and forward compatibility
- Protobuf/Thrift

### Sample

You can follow the detailed example below:

- Take pb as an example. First, define an `example.proto` file with the ServiceName as `Example`.
- The name of the rpc interface is `Echo`, with the input parameter as `EchoRequest`, and the output parameter as `EchoResponse`.
- `EchoRequest` consists of two strings: `message` and `name`.
- `EchoResponse` consists of one string: `message`.

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

- It is the basic unit for sogouRPC services.
- Each service must be generated by one type of IDLs.
- Service is determined by IDL type, not by specific network communication protocol.

### Sample

You can follow the detailed example below:

- Use the same `example.proto` IDL above.
- Run the official `protoc example.proto --cpp_out=./ --proto_path=./` to get two files: `example.pb.h` and `example.pb.cpp`.
- Run the `srpc_generator protobuf ./example.proto ./` in SogouRPC to get `example.srpc.h`.
- Derive `Example::Service` to implement the rpc business logic, which is an RPC Service.
- Please note that this Service does not involve any concepts such as network, port, communication protocol, etc., and it is only responsible for completing the business logic that convert `EchoRequest` to `EchoResponse`.

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

- Each server corresponds to one port
- Each server corresponds to one specific network communication protocol
- One service may be added into multiple Servers
- One Server may have one or more Services, but the ServiceName must be unique within that Server
- Services from different IDLs can be added into the same Server

### Sample

You can follow the detailed example below:

- Follow the above `ExampleServiceImpl` Service
- First, create an RPC Server and determine the proto file.
- Then, create any number of Service instances and any number of Services for different protocols, and add these services to the Server through the `add_service()`interface.
- Finally, use `start()` or `serve()` to start the services in the Server and handle the upcoming rpc requests through the Server.
- Imagine that we can also derive more Serivce from `Example::Service`, which have different implementations of rpc `Echo`.
- Imagine that we can create N different RPC Servers on N different ports, serving on different network protocols.
- Imagine that we can use `add_service()` to add the same ServiceIMPL instance on different Servers, or we can use `add_service()` to add different ServiceIMPL instances on the same server.
- Imagine that we can use the same `ExampleServiceImpl`, serving BPRC-STD, SogouRPC-STD, SogouRPC-Http at three different ports at the same time.
- And we can use `add_service()` to add one `ExampleServiceImpl` related to Protobuf IDL and one `AnotherThriftServiceImpl` related to Thrift IDL to the same SogouRPC-STD Server, and the two IDLs work perfectly on the same port!

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

- Each Client corresponds to one specific target/one specific cluster
- Each Client corresponds to one specific network communication protocol
- Each Client corresponds to one specific IDL

### Sample

You can follow the detailed example below:

- Following the above example, the client is relatively simple and you can call the method directly.
- Use `Example::XXXClient` to create a client instance of some RPC. The IP+port or URL of the target is required.
- With the client instance, directly call the rpc function `Echo`. This is an asynchronous request, and the callback function will be invoked after the request is completed.
- For the usage of the RPC Context, please check [RPC Context](/docs/en/rpc.md#rpc-context).

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

- RPCContext is used specially to assist asynchronous interfaces, and can be used in both Service and Client.
- Each asynchronous interface will provide a Context, which offers higher-level functions, such as obtaining the remote IP, the connection seqid, and so on.
- Some functions on Context are unique to Server or Client. For example, you can set the compression mode of the response data on Server, and you can obtain the success or failure status of a request on Client.
- On the Context, you can use ``get_series()`` to obtain the SeriesWork, which is seamlessly integrated with the asynchronous mode of Workflow.

### RPCContext API - Common

#### `long long get_seqid() const;`

One complete communication consists of request+response. The sequence id of the communication on the current socket connection can be obtained, and seqid=0 indicates the first communication.

#### `std::string get_remote_ip() const;`

Get the remote IP address. IPv4/IPv6 is supported.

#### `int get_peer_addr(struct sockaddr *addr, socklen_t *addrlen) const;`

Get the remote address. The in/out parameter is the lower-level data structure sockaddr.

#### `const std::string& get_service_name() const;`

Get RPC Service Name.

#### `const std::string& get_method_name() const;`

Get RPC Method Name.

#### `SeriesWork *get_series() const;`

Get the SeriesWork of the current ServerTask/ClientTask.

### RPCContext API - Only for client done

#### `bool success() const;`

For client only. The success or failure of the request.

#### `int get_status_code() const;`

For client only. The rpc status code of the request.

#### `const char *get_errmsg() const;`

For client only. The error info of the request.

#### `int get_error() const;`

For client only. The error code of the request.

#### `void *get_user_data() const;`

For client only. Get the user\_data of the ClientTask. If a user generates a task through the ``create_xxx_task()`` interface, the context can be recorded in the user_data field. You can set that field when creating the task, and retrieve it in the callback function.

### RPCContext API - Only for server process

#### `void set_data_type(RPCDataType type);`

For Server only. Set the data packaging type

- RPCDataProtobuf
- RPCDataThrift
- RPCDataJson

#### `void set_compress_type(RPCCompressType type);`

For Server only. Set the data compression type (note: the compression type for the Client is set on Client or Task)

- RPCCompressNone
- RPCCompressSnappy
- RPCCompressGzip
- RPCCompressZlib
- RPCCompressLz4

#### `void set_attachment_nocopy(const char *attachment, size_t len);`

For Server only. Set the attachment.

#### `bool get_attachment(const char **attachment, size_t *len) const;`

For Server only. Get the attachment.

#### `void set_reply_callback(std::function<void (RPCContext *ctx)> cb);`

For Server only. Set reply callback, which is called after the operating system successfully writes the data into the socket buffer.

#### `void set_send_timeout(int timeout);`

For Server only. Set the maximum time for sending the message, in milliseconds. -1 indicates unlimited time.

#### `void set_keep_alive(int timeout);`

For Server only. Set the maximum connection keep-alive time,  in milliseconds. -1 indicates unlimited time.

## RPC Options

### Server Parameters

| Name                     | Default value               | Description                                                  |
| ------------------------ | --------------------------- | ------------------------------------------------------------ |
| max\_connections         | 2000                        | The maximum number of connections for the Server. The default value is 2000. |
| peer\_response\_timeout: | 10\* 1000                   | The maximum time for each read IO. The default value is 10 seconds. |
| receive\_timeout:        | -1                          | The maximum read time of each complete message. -1 indicates unlimited time. |
| keep\_alive\_timeout:    | 60 \* 1000                  | Maximum keep_alive time for idle connection. -1 indicates no disconnection. 0 indicates short connection. The default keep-alive time for long connection  is 60 seconds |
| request\_size\_limit     | 2LL \* 1024 \* 1024 \* 1024 | Request packet size limit. Maximum: 2 GB                     |
| ssl\_accept\_timeout:    | 10 \* 1000                  | SSL connection timeout. The default value is 10 seconds.      |

### Client Parameters

| Name         | Default value                       | Description                                                  |
| ------------ | ----------------------------------- | ------------------------------------------------------------ |
| host         | ""                                  | Target host, which can be an IP address or a domain name     |
| port         | 1412                                | Destination port number. The default value is 1412           |
| is\_ssl      | false                               | SSL switch. The default value is off                         |
| url          | ""                                  | The URL is valid only when the host is empty. URL will override three items: host/port/is\_ssl |
| task\_params | The default configuration for tasks | See the configuration items below                            |

### Task Parameters

| Name                 | Default value    | Description                                                  |
| -------------------- | ---------------- | ------------------------------------------------------------ |
| send\_timeout        | -1               | Maximum time for sending the message. The default value is unlimited time. |
| receive\_timeout     | -1               | Maximum time for sending a response. The default value is unlimited time. |
| watch\_timeout       | 0                | The maximum time of the first reply from remote. The default value is 0, indicating unlimited time. |
| keep\_alive\_timeout | 30 \* 1000       | The maximum keep-alive time for idle connections. -1 indicates no disconnection. The default value is 30 seconds. |
| retry\_max           | 0                | Maximum number of retries. The default value is 0, indicating no retry. |
| compress\_type       | RPCCompressNone  | Compression type. The default value is no compression.       |
| data\_type           | RPCDataUndefined | The data type of network packets. The default value is consistent with the default value for RPC. For SRPC-Http protocol, it is JSON, and for others, they are the corresponding IDL types. |

## Integrating with the asynchronous Workflow framework

### Server

You can follow the detailed example below:

- Echo RPC sends an HTTP request to the upstream modules when it receives the request.
- After the request to the upstream modules is completed, the server populates the body of HTTP response into the message of the response and send a reply to the client.
- We don't want to block/occupy the handler thread, so the request to the upstream must be asynchronous.
- First, we can use `WFTaskFactory::create_http_task()` of the factory of Workflow to create an asynchronous http_task.
- Then, we use `ctx->get_series()` of the RPCContext to get the SeriesWork of the current ServerTask.
- Finally, we use the `push_back()` interface of the SeriesWork to append the http\_task to the SeriesWork.

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

You can follow the detailed example below:

- We send two requests in parallel. One is an RPC request and the other is an HTTP request.
- After both requests are finished, we initiate a calculation task again to calculate the sum of the squares of the two numbers.
- First, use `create_Echo_task()` of the RPC Client to create an rpc\_task, which is an asynchronous RPC network request.
- Then, use `WFTaskFactory::create_http_task` and `WFTaskFactory::create_go_task` in the the factory of Workflow to create an asynchronous network task http\_task and an asynchronous computing task calc\_task respectively.
- Finally, use the serial-parallel graph to organize three asynchronous tasks, in which the multiplication sign indicates parallel tasks and the greater than sign indicates serial tasks and then execute ``start()``.

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

