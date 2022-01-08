[English version](/docs/en/tutorial-01-idl.md)

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

### 关于HTTP请求路径拼接
基于以上proto文件所写的[SRPCHttpServer](https://github.com/sogou/srpc#3-servercc)和[TRPCHttpServer]((../tutorial/tutorial-13-trpc_http_server.cc))，除了用*SRPCHttpClient*或*TRPCHttpClient*以外，我们还可以通过其他工具(比如curl）或者其他语言（比如python）发送Http请求进行互通的，这样可以轻松实现跨语言。以上proto文件的`EchoRequest`可以通过*curl*用这样的命令发送：
```sh
curl 127.0.0.1:8811/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
```

其中，如果*proto*文件中带有*package*名，比如：
```proto
package workflow.srpc.tutorial;
```

则需要注意与*SRPCHttpServer*互通时，HTTP路径需要把package如下拼接：
```sh
curl 127.0.0.1:8811/workflow/srpc/tutorial/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
```

而与*TRPCHttpServer*互通时，路径如下拼接：
```sh
curl 127.0.0.1:8811/workflow.srpc.tutorial.Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
```

