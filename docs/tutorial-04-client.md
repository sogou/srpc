[English version](/docs/en/tutorial-04-client.md)

## RPC Client
- 每一个Client对应着一个确定的目标/一个确定的集群
- 每一个Client对应着一个确定的网络通信协议
- 每一个Client对应着一个确定的IDL

### 示例
下面我们通过一个具体例子来呈现
- 沿用上面的例子，client相对简单，直接调用即可
- 通过``Example::XXXClient``创建某种RPC的client实例，需要目标的ip+port或url
- 利用client实例直接调用rpc函数``Echo``即可，这是一次异步请求，请求完成后会进入回调函数
- 具体的RPC Context用法请看下一个段落

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

