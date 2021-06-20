[中文版](/docs/tutorial-04-client.md)

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

