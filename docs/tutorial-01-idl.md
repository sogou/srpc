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

