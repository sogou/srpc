#include <signal.h>
#include <stdio.h>
#include "workflow/%sMessage.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    const char *url = "%s://127.0.0.1:%s"; // proxy address

    WF%sTask *task = WFTaskFactory::create_%s_task(url,%s
                                                         1, // retry
                                                         [](WF%sTask *task) // callback
    {
        int state = task->get_state();
        int error = task->get_error();
        fprintf(stderr, "%s client state = %%d error = %%d\n", state, error);
%s
    });
%s
    task->start();

    wait_group.wait();
    return 0;
}

