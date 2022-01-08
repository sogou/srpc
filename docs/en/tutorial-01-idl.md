[中文版](/docs/tutorial-01-idl.md)

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

### About splicing the path of HTTP request

Base on the proto file above, we can start HTTP servers like [SRPCHttpServer](https://github.com/sogou/srpc#3-servercc) and [TRPCHttpServer]((../../tutorial/tutorial-13-trpc_http_server.cc)). To communicate with the HTTP servers,  we can not only use *SRPCHttpClient* or *TRPCHttpClient*, but also use other tools (such as curl) or other language (such as python).  It's a very common realization about cross-language. For example, the `EchoRequest` can sent by *curl* as follows:

```sh
curl 127.0.0.1:8811/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
```

Additionally, if the *proto* file has a *package* name, such as:
```proto
package workflow.srpc.tutorial;
```

When communicate to *SRPCHttpServer*, we should splice the package name like this:
```sh
curl 127.0.0.1:8811/workflow/srpc/tutorial/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
```

While communicate to *TRPCHttpServer*, we should splice the package name like this:
```sh
curl 127.0.0.1:8811/workflow.srpc.tutorial.Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
```

