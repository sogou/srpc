#include <stdio.h>
#include "workflow/WFFacilities.h"
#include "srpc/rpc_types.h"
#include "%s.srpc.h"
#include "config/config.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);
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
    EchoResult res;

    client.Echo(res, "Hello, this is sync request!");

    if (client.thrift_last_sync_success())
        fprintf(stderr, "sync resp. %%s\n", res.message.c_str());
    else
    {
        const auto& sync_ctx = client.thrift_last_sync_ctx();

        fprintf(stderr, "sync status[%%d] error[%%d] errmsg:%%s\n",
                sync_ctx.status_code, sync_ctx.error, sync_ctx.errmsg.c_str());
    }

    // 4. request with async api
    %s::EchoRequest req;
    req.message = "Hello, this is async request!";

    client.Echo(&req, [](%s::EchoResponse *resp, RPCContext *ctx) {
        if (ctx->success())
            fprintf(stderr, "async resp. %%s\n", resp->result.message.c_str());
        else
            fprintf(stderr, "async status[%%d] error[%%d] errmsg:%%s\n",
                    ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
        wait_group.done();
    });

    wait_group.wait();

    return 0;
}

