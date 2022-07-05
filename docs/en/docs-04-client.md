[中文版](/docs/docs-04-client.md)

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

### Client startup parameters

Client can be started directly by passing in ip, port, or through client startup parameters.

The above example:

~~~cpp
Example::SRPCClient client("127.0.0.1", 1412);
~~~

is equivalent to:

~~~cpp
struct RPCClientParams param = RPC_CLIENT_PARAMS_DEFAULT;
param.host = "127.0.0.1";
param.port = 1412;
Example::SRPCClient client(&param);
~~~

also equivalent to:

~~~cpp
struct RPCClientParams param = RPC_CLIENT_PARAMS_DEFAULT;
param.url = "srpc://127.0.0.1:1412";
Example::SRPCClient client(&param);
~~~

Note that `RPC_CLIENT_PARAMS_DEFAULT` must be used to initialize the client's parameters, which contains a `RPCTaskParams`, including the default data_type, compress_type, retry_max and various timeouts. The specific struct can refer to [rpc_options.h](/src/rpc_options.h).

