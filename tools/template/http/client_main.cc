#include <signal.h>
#include <stdio.h>
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
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
    if (config.load("./config.json") == false)
    {
        perror("Load config failed");
        exit(1);
    }

    signal(SIGINT, sig_handler);
}

void callback(WFHttpTask *task)
{
    protocol::HttpRequest *req = task->get_req();
    protocol::HttpResponse *resp = task->get_resp();
    int state = task->get_state();
    int error = task->get_error();

    printf("http client callback. state = %d", state);
    if (state != WFT_STATE_SUCCESS)
    {
        printf(" error = %d\n", error);
        return;
    }
    printf("\n");

    std::string name;
    std::string value;

    /* Print request. */
    fprintf(stderr, "%s %s %s\r\n", req->get_method(),
                                    req->get_http_version(),
                                    req->get_request_uri());

    protocol::HttpHeaderCursor req_cursor(req);

    while (req_cursor.next(name, value))
        fprintf(stderr, "%s: %s\r\n", name.c_str(), value.c_str());
    fprintf(stderr, "\r\n");

    /* Print response header. */
    fprintf(stderr, "%s %s %s\r\n", resp->get_http_version(),
                                    resp->get_status_code(),
                                    resp->get_reason_phrase());

    protocol::HttpHeaderCursor resp_cursor(resp);

    while (resp_cursor.next(name, value))
        fprintf(stderr, "%s: %s\r\n", name.c_str(), value.c_str());
    fprintf(stderr, "\r\n");

    /* Print response body. */
    const void *body;
    size_t body_len;

    resp->get_parsed_body(&body, &body_len);
    fwrite(body, 1, body_len, stdout);
    fflush(stdout);

    fprintf(stderr, "\n");
}

int main()
{
    init();

    std::string url = std::string("http://") + config.client_host() + 
                      std::string(":") + std::to_string(config.client_port());

    WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                       config.redirect_max(),
                                                       config.retry_max(),
                                                       callback);
    protocol::HttpRequest *req = task->get_req();
    req->append_output_body("This is http client.");
    task->start();

    wait_group.wait();
    return 0;
}

