#include <stdio.h>
#include "srpc/rpc_types.h"
#include "%s.srpc.h"
#include "config/config.h"

using namespace srpc;

static srpc::RPCConfig config;

void init()
{
    if (config.load("./client.conf") == false)
    {
        perror("Load config failed");
        exit(1);
    }
}

int main()
{
    // 1. load config
    init();

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
        fprintf(stderr, "%%s\n", resp.DebugString().c_str());
    else
        fprintf(stderr, "status[%%d] error[%%d] errmsg:%%s\n",
                ctx.status_code, ctx.error, ctx.errmsg.c_str());

    return 0;
}

