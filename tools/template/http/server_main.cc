#include <stdio.h>
#include <signal.h>
#include "workflow/WFServer.h"
#include "workflow/WFHttpServer.h"
#include "workflow/WFFacilities.h"

#include "config/config.h"
#include "server_example.h"

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

void process(WFHttpTask *server_task)
{
    // delete the example codes and fill your logic
    process_example(server_task);
}

int main()
{
    init();

    WFHttpServer server(process);

    if (server.start(config.server_port()) == 0)
    {
        printf("http server start, port %u\n", config.server_port());
        wait_group.wait();
        server.stop();
    }

    return 0;
}

