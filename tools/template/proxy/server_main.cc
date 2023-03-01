#include <stdio.h>
#include <signal.h>
#include "workflow/WF%sServer.h"
#include "workflow/WFFacilities.h"

#include "util.h"

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    unsigned short port = %s;

    WF%sServer server([](WF%sTask *task)
    {
        // delete the example codes and fill your logic
%s
    });

    if (server.start(port) == 0)
    {
        fprintf(stderr, "%s server start, port %%u\n", port);
        wait_group.wait();
        server.stop();
    }

    return 0;
}

