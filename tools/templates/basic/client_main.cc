#include <stdio.h>
#include <signal.h>
#include "workflow/%sMessage.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"

#include "config/config.h"

static WFFacilities::WaitGroup wait_group(1);
static srpc::RPCConfig config;

void sig_handler(int signo)
{
    wait_group.done();
}

void init()
{
    if (config.load("./client.conf") == false)
    {
        perror("Load config failed");
        exit(1);
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
}

void callback(WF%sTask *task)
{
    int state = task->get_state();
    int error = task->get_error();
    fprintf(stderr, "%s client state = %%d error = %%d\n", state, error);
%s
}

int main()
{
    init();

    std::string url = std::string("%s://") + config.client_host() + 
                      std::string(":") + std::to_string(config.client_port());


    WF%sTask *task = WFTaskFactory::create_%s_task(url,%s
                                                        config.retry_max(),
                                                        callback);
%s
    task->start();

    wait_group.wait();
    return 0;
}

