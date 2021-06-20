[中文版](/docs/tutorial-03-server.md)

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

