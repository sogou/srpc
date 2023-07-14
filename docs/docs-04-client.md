[English version](/docs/en/docs-04-client.md)

## 04 - RPC Client
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
#include "workflow/WFFacilities.h"

using namespace srpc;

int main()
{
    Example::SRPCClient client("127.0.0.1", 1412);
    EchoRequest req;
    req.set_message("Hello!");
    req.set_name("SRPCClient");

    WFFacilities::WaitGroup wait_group(1);

    client.Echo(&req, [&wait_group](EchoResponse *response, RPCContext *ctx) {
        if (ctx->success())
            printf("%s\n", response->DebugString().c_str());
        else
            printf("status[%d] error[%d] errmsg:%s\n",
                    ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
        wait_group.done();
    });

    wait_group.wait();
    return 0;
}
~~~

### 启动参数

Client可以直接通过传入ip、port启动，或者通过参数启动。

上面的例子：

~~~cpp
Example::SRPCClient client("127.0.0.1", 1412);
~~~

等同于：

~~~cpp
struct RPCClientParams param = RPC_CLIENT_PARAMS_DEFAULT;
param.host = "127.0.0.1";
param.port = 1412;
Example::SRPCClient client(&param);
~~~

也等同于：

~~~cpp
struct RPCClientParams param = RPC_CLIENT_PARAMS_DEFAULT;
param.url = "srpc://127.0.0.1:1412";
Example::SRPCClient client(&param);
~~~

注意这里一定要使用`RPC_CLIENT_PARAMS_DEFAULT`去初始化我们的参数，里边包含了一个`RPCTaskParams`，包括默认的data_type、compress_type、重试次数和多种超时，具体结构可以参考[rpc_options.h](/src/rpc_options.h)。

