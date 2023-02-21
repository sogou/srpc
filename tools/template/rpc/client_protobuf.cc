#include <stdio.h>
#include "srpc/rpc_types.h"
#include "%s.srpc.h"
#include "config/config.h"

using namespace srpc;

int main()
{
    // 1. initialize
    srpc::RPCConfig config;
    if (config.load("./config.json") == false)
    {
        perror("Load config failed");
        exit(1);
    }
    
    // 2. start client
    RPCClientParams params = RPC_CLIENT_PARAMS_DEFAULT;
    params.host = config.client_host();
    params.port = config.client_port();
%s
    %s::%sClient client(&params);
    config.load_filter(client);

    // 3. request with sync api
    EchoRequest req;
    EchoResponse resp;
    RPCSyncContext ctx;

    req.set_message("Hello, srpc!");
    client.Echo(&req, &resp, &ctx);

    if (ctx.success)
        printf("%%s\n", resp.DebugString().c_str());
    else
        printf("status[%%d] error[%%d] errmsg:%%s\n",
               ctx.status_code, ctx.error, ctx.errmsg.c_str());

    return 0;
}

