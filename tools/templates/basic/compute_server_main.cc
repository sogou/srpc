#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "workflow/WFHttpServer.h"
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
    if (config.load("./server.conf") == false)
    {
        perror("Load config failed");
        exit(1);
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
}

// Example for Fibonacci.
void Fibonacci(int n, protocol::HttpResponse *resp)
{
    unsigned long long x = 0, y = 1;
    char buf[256];
    int i;

    if (n <= 0 || n > 94)
    {
        resp->append_output_body_nocopy("<html>Invalid Number.</html>",
                                 strlen("<html>Invalid Number.</html>"));
        return;
    }

    resp->append_output_body_nocopy("<html>", strlen("<html>"));
    for (i = 2; i < n; i++)
    {
        sprintf(buf, "<p>%llu + %llu = %llu.</p>", x, y, x + y);
        resp->append_output_body(buf);
        y = x + y;
        x = y - x;
    }

    if (n == 1)
        y = 0;
    sprintf(buf, "<p>The No. %d Fibonacci number is: %llu.</p>", n, y);
    resp->append_output_body(buf);
    resp->append_output_body_nocopy("</html>", strlen("</html>"));
}

void process(WFHttpTask *task)
{
    const char *uri = task->get_req()->get_request_uri();
    if (*uri == '/')
        uri++;

    int n = atoi(uri);
    protocol::HttpResponse *resp = task->get_resp();
    fprintf(stderr, "server get request. n = %d\n", n);

    // All calculations can be encapsulated by 'go_task'
    WFGoTask *go_task = WFTaskFactory::create_go_task("go", Fibonacci, n, resp);

    // Tasks will be dispatch asynchonously after 'push_back()'
    series_of(task)->push_back(go_task);
}

int main()
{
    init();

    WFHttpServer server(process);

    if (server.start(config.server_port()) == 0)
    {
        fprintf(stderr, "Computing server started, port %u\n", config.server_port());
        wait_group.wait();
        server.stop();
    }
    else
        perror("server start");

    return 0;
}

