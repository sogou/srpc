#include <stdio.h>
#include "srpc/rpc_types.h"
#include "%s.srpc.h"
#include "config/config.h"

using namespace srpc;

static srpc::RPCConfig config;

void init()
{
    if (config.load("./config.json") == false)
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
    EchoResult res;

    client.Echo(res, "Hello, srpc!");

    if (client.thrift_last_sync_success())
        printf("%%s\n", res.message.c_str());
    else
    {
        const auto& sync_ctx = client.thrift_last_sync_ctx();

        printf("status[%%d] error[%%d] errmsg:%%s\n",
                sync_ctx.status_code, sync_ctx.error, sync_ctx.errmsg.c_str());
    }

    return 0;
}

