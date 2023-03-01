#include <stdio.h>
#include <signal.h>
#include "%s.srpc.h"
#include "workflow/WFFacilities.h"
#include "srpc/rpc_types.h"
#include "config/config.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);
static srpc::RPCConfig config;

void sig_handler(int signo)
{
    wait_group.done();
}

void init()
{
    if (config.load("./config.json") == false)
    {
        perror("Load config failed");
        exit(1);
    }

    signal(SIGINT, sig_handler);
}

class %sServiceImpl : public %s::Service
{
public:
    void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
    {
%s// 4. delete the following codes and fill your logic
        resp->set_message("Hi back");
    }
};

int main()
{
    // 1. load config
    init();

    // 2. start server
    %sServer server;
    %sServiceImpl impl;
    server.add_service(&impl);

    config.load_filter(server);

    if (server.start(config.server_port()) == 0)
    {
        // 3. success and wait
        printf("%s %s server start, port %%u\n", config.server_port());
        wait_group.wait();
        server.stop();
    }

    return 0;
}

