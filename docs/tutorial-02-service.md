[English version](/docs/en/tutorial-02-service.md)

## RPC Service
- 组成sogouRPC服务的基本单元
- 每一个Service一定由某一种IDL生成
- Service只与IDL有关，与网络通信具体协议无关

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

