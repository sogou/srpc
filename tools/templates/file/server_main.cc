#include <stdio.h>
#include <signal.h>
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFHttpServer.h"
#include "workflow/WFFacilities.h"

#include "config/config.h"
#include "file_service.h"

using namespace protocol;

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
}

int main()
{
    init();

    unsigned short port = config.server_port();
    const char *cert_file = config.server_cert_file();
    const char *file_key = config.server_file_key();

    FileService service(config.get_root_path(), config.get_error_page());
    auto&& proc = std::bind(&FileService::process, &service,
                            std::placeholders::_1);

    int ret;
    WFHttpServer server(proc);

    if (strlen(cert_file) != 0 && strlen(file_key) != 0)
        ret = server.start(port, cert_file, file_key);
    else
        ret = server.start(port);

    if (ret == 0)
    {
        fprintf(stderr, "http file service start, port %u\n", port);
        wait_group.wait();
        server.stop();
    }
    else
        perror("http file service start");

    return 0;
}

