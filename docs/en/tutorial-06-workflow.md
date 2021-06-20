[中文版](/docs/tutorial-06-workflow.md)

## Integrating with the asynchronous Workflow framework

### Server

You can follow the detailed example below:

- Echo RPC sends an HTTP request to the upstream modules when it receives the request.
- After the request to the upstream modules is completed, the server populates the body of HTTP response into the message of the response and send a reply to the client.
- We don't want to block/occupy the handler thread, so the request to the upstream must be asynchronous.
- First, we can use `WFTaskFactory::create_http_task()` of the factory of Workflow to create an asynchronous http_task.
- Then, we use `ctx->get_series()` of the RPCContext to get the SeriesWork of the current ServerTask.
- Finally, we use the `push_back()` interface of the SeriesWork to append the http\_task to the SeriesWork.

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
    void Echo(EchoRequest *request, EchoResponse *response, RPCContext *ctx) override
    {
        auto *http_task = WFTaskFactory::create_http_task("https://www.sogou.com", 0, 0,
            [request, response](WFHttpTask *task) {
                if (task->get_state() == WFT_STATE_SUCCESS)
                {
                    const void *data;
                    size_t len;
                    task->get_resp()->get_parsed_body(&data, &len);
                    response->mutable_message()->assign((const char *)data, len);
                }
                else
                    response->set_message("Error: " + std::to_string(task->get_error()));

                printf("Server Echo()\nget_req:\n%s\nset_resp:\n%s\n",
                                            request->DebugString().c_str(),
                                            response->DebugString().c_str());
            });

        ctx->get_series()->push_back(http_task);
    }
};
~~~

### Client

You can follow the detailed example below:

- We send two requests in parallel. One is an RPC request and the other is an HTTP request.
- After both requests are finished, we initiate a calculation task again to calculate the sum of the squares of the two numbers.
- First, use `create_Echo_task()` of the RPC Client to create an rpc\_task, which is an asynchronous RPC network request.
- Then, use `WFTaskFactory::create_http_task` and `WFTaskFactory::create_go_task` in the the factory of Workflow to create an asynchronous network task http\_task and an asynchronous computing task calc\_task respectively.
- Finally, use the serial-parallel graph to organize three asynchronous tasks, in which the multiplication sign indicates parallel tasks and the greater than sign indicates serial tasks and then execute ``start()``.

~~~cpp
void calc(int x, int y)
{
    int z = x * x + y * y;

    printf("calc result: %d\n", z);
}

int main()
{
    Example::SRPCClient client("127.0.0.1", 1412);

    auto *rpc_task = client.create_Echo_task([](EchoResponse *response, RPCContext *ctx) {
        if (ctx->success())
            printf("%s\n", response->DebugString().c_str());
        else
            printf("status[%d] error[%d] errmsg:%s\n",
                    ctx->get_status_code(), ctx->get_error(), ctx->get_errmsg());
    });

    auto *http_task = WFTaskFactory::create_http_task("https://www.sogou.com", 0, 0, [](WFHttpTask *task) {
        if (task->get_state() == WFT_STATE_SUCCESS)
        {
            std::string body;
            const void *data;
            size_t len;

            task->get_resp()->get_parsed_body(&data, &len);
            body.assign((const char *)data, len);
            printf("%s\n\n", body.c_str());
        }
        else
            printf("Http request fail\n\n");
    });

    auto *calc_task = WFTaskFactory::create_go_task(calc, 3, 4);

    EchoRequest req;
    req.set_message("Hello, sogou rpc!");
    req.set_name("1412");
    rpc_task->serialize_input(&req);

    ((*http_task * rpc_task) > calc_task).start();

    pause();
    return 0;
}
~~~

