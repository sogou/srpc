#include <stdio.h>
#include <signal.h>
#include "workflow/WF%sServer.h"
#include "workflow/WFFacilities.h"

#include "config/util.h"
#include "config/config.h"

static WFFacilities::WaitGroup wait_group(1);
static srpc::RPCConfig config;

void sig_handler(int signo)
{
    wait_group.done();
}

void init()
{
    if (config.load("./server.conf") == false)
    {
        perror("Load config failed");
        exit(1);
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
}

void process(WF%sTask *task)
{
    // delete the example codes and fill your logic
%s
}

int main()
{
    init();

    WF%sServer server(process);

    if (server.start(config.server_port()) == 0)
    {
        fprintf(stderr, "%s server started, port %%u\n", config.server_port());
        wait_group.wait();
        server.stop();
    }
    else
        perror("server start");

    return 0;
}

