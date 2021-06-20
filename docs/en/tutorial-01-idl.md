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

