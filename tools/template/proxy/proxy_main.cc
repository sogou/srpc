#include <stdio.h>
#include <signal.h>
#include "workflow/WFTaskFactory.h"
#include "workflow/WF%sServer.h"
#include "workflow/WFFacilities.h"

#include "config/config.h"
#include "util.h"

static WFFacilities::WaitGroup wait_group(1);
static srpc::RPCConfig config;

void sig_handler(int signo)
{
    wait_group.done();
}

void init()
{
    if (config.load("./proxy.conf") == false)
    {
        perror("Load config failed");
        exit(1);
    }

    signal(SIGINT, sig_handler);
}

void callback(WF%sTask *client_task)
{
    int state = client_task->get_state();
    int error = client_task->get_error();
    SeriesWork *series = series_of(client_task);
    protocol::%sResponse *resp = client_task->get_resp();
    protocol::%sResponse *proxy_resp = (protocol::%sResponse *)series->get_context();

    // Copy the remote server's response, to proxy response.
    if (state == WFT_STATE_SUCCESS)%s
    fprintf(stderr, "backend server state = %%d error = %%d. response client.\n",
            state, error);
}

void process(WF%sTask *server_task)
{
    protocol::%sRequest *req = server_task->get_req();
    std::string backend_server = config.client_host();
    unsigned short backend_server_port = config.client_port();
    std::string url = std::string("%s://") + backend_server +
                      std::string(":") + std::to_string(backend_server_port);

    WF%sTask *client_task = WFTaskFactory::create_%s_task(url,%s
                                                                config.retry_max(),
                                                                callback);

    // Copy user's request to the new task's request using std::move()
%s
    SeriesWork *series = series_of(server_task);
    series->set_context(server_task->get_resp());
    series->push_back(client_task);

    fprintf(stderr, "proxy get request from client: ");
    print_peer_address<WF%sTask>(server_task);
}

int main()
{
    init();

    WF%sServer proxy_server(process);

    if (proxy_server.start(config.server_port()) == 0)
    {
        fprintf(stderr, "[%s]-[%s] proxy start, port %%u\n", config.server_port());
        wait_group.wait();
        proxy_server.stop();
    }

    return 0;
}

