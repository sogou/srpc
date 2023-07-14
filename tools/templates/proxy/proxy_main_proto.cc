#include <stdio.h>
#include <signal.h>

#include "workflow/WFFacilities.h"
#include "srpc/rpc_types.h"

#include "config/config.h"
#include "%s.srpc.h"

using namespace srpc;

static srpc::RPCConfig config;
static WFFacilities::WaitGroup wait_group(1);

static void sig_handler(int signo)
{
    wait_group.done();
}

static void init()
{
    if (config.load("./proxy.conf") == false)
    {
        perror("Load config failed");
        exit(1);
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
}

class ProxyServiceImpl : public %s::Service
{
public:
    void Echo(EchoRequest *request, EchoResponse *response,
              RPCContext *context) override
    {
        fprintf(stderr, "%s proxy get request from client. ip : %%s\n%%s\n",
                context->get_remote_ip().c_str(), request->DebugString().c_str());

        // 5. process() : get request from client and send to remote server
        auto *task = this->client.create_Echo_task([response](EchoResponse *resp,
                                                              RPCContext *ctx) {
            // 6. callback() : fill remote server response to client
            if (ctx->success())
                *response = std::move(*resp);
        });
        task->serialize_input(request);
        context->get_series()->push_back(task);
    }

public:
    ProxyServiceImpl(RPCClientParams *params) :
        client(params)
    {
    }

private:
    %s::%sClient client;
};

int main()
{
    // 1. load config
    init();

    // 2. make client for remote server
    RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
    client_params.host = config.client_host();
    client_params.port = config.client_port();

    // 3. start proxy server
    %sServer server;
    ProxyServiceImpl impl(&client_params);
    server.add_service(&impl);
    config.load_filter(server);

    if (server.start(config.server_port()) == 0)
    {
        // 4. main thread success and wait
        fprintf(stderr, "%s [%s]-[%s] proxy started, port %%u\n",
                config.server_port());
        wait_group.wait();
        server.stop();
    }
    else
        perror("server start");

    return 0;
}

