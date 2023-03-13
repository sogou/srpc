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
    EchoRequest req;
    EchoResponse resp;
    RPCSyncContext ctx;

    req.set_message("Hello, this is sync request!");
    client.Echo(&req, &resp, &ctx);

    if (ctx.success)
        fprintf(stderr, "sync resp. %%s\n", resp.DebugString().c_str());
    else
        fprintf(stderr, "sync status[%%d] error[%%d] errmsg:%%s\n",
                ctx.status_code, ctx.error, ctx.errmsg.c_str());

    // 4. request with async api

    req.set_message("Hello, this is async request!");

    client.Echo(&req, [](EchoResponse *resp, RPCContext *ctx) {
        if (ctx->success())
            fprintf(stderr, "async resp. %%s\n", resp->DebugString().c_str());
        else
            fprintf(stderr, "async status[%%d] error[%%d] errmsg:%%s\n",
                    ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
        wait_group.done();
    });

    wait_group.wait();

    return 0;
}

