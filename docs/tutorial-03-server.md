[English version](/docs/en/tutorial-03-server.md)

## RPC Server
- 每一个Server对应一个端口
- 每一个Server对应一个确定的网络通信协议
- 每一个Service可以添加到任意的Server里
- 每一个Server可以拥有任意的Service，但在当前Server里ServiceName必须唯一
- 不同IDL的Service是可以放进同一个Server中的

### 示例
下面我们通过一个具体例子来呈现
- 沿用上面的``ExampleServiceImpl``Service
- 首先，我们创建1个RPC Server、需要确定协议
- 然后，我们可以创建任意个数的Service实例、任意不同proto形成的Service，把这些Service通过``add_service()``接口添加到Server里
- 最后，通过Server的``start``或者``serve``开启服务，处理即将到来的rpc请求
- 想像一下，我们也可以从``Example::Service``派生更多的功能的rpc``Echo``不同实现的Service
- 想像一下，我们可以在N个不同的端口创建N个不同的RPC Server、代表着不同的协议
- 想像一下，我们可以把同一个ServiceIMPL实例``add_service``到不同的Server上，我们也可以把不同的ServiceIMPL实例``add_service``到同一个Server上
- 想像一下，我们可以用同一个``ExampleServiceImpl``，在三个不同端口、同时服务于BPRC-STD、SogouRPC-STD、SogouRPC-Http
- 甚至，我们可以将1个PB的``ExampleServiceImpl``和1个Thrift的``AnotherThriftServiceImpl``，``add_service``到同一个SogouRPC-STD Server，两种IDL在同一个端口上完美工作！

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

