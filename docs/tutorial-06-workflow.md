[English version](/docs/en/tutorial-06-workflow.md)

## 与workflow异步框架的结合
### 1. Server
下面我们通过一个具体例子来呈现
- Echo RPC在接收到请求时，向下游发起一次http请求
- 对下游请求完成后，我们将http response的body信息填充到response的message里，回复给客户端
- 我们不希望阻塞/占据着Handler的线程，所以对下游的请求一定是一次异步请求
- 首先，我们通过Workflow框架的工厂``WFTaskFactory::create_http_task``创建一个异步任务http_task
- 然后，我们利用RPCContext的``ctx->get_series()``获取到ServerTask所在的SeriesWork
- 最后，我们使用SeriesWork的``push_back``接口将http_task放到SeriesWork的后面

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

### 2. Client
下面我们通过一个具体例子来呈现
- 我们并行发出两个请求，1个是rpc请求，1个是http请求
- 两个请求都结束后，我们再发起一次计算任务，计算两个数的平方和
- 首先，我们通过RPC Client的``create_Echo_task``创建一个rpc异步请求的网络任务rpc_task
- 然后，我们通过Workflow框架的工厂``WFTaskFactory::create_http_task``和``WFTaskFactory::create_go_task``分别创建异步网络任务http_task，和异步计算任务calc_task
- 最后，我们利用串并联流程图，乘号代表并行、大于号代表串行，将3个异步任务组合起来执行start

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

### 3. Upstream
SRPC可以直接使用Workflow的任何组件，最常用的就是[Upstream](https://github.com/sogou/workflow/blob/master/docs/about-upstream.md)，SRPC的任何一种client都可以使用Upstream。

我们通过参数来看看如何构造可以使用Upstream的client：

```cpp
#include "workflow/UpstreamManager.h"

int main()
{
    // 1. 创建upstream并添加实例
    UpstreamManager::upstream_create_weighted_random("echo_server", true);
    UpstreamManager::upstream_add_server("echo_server", "127.0.0.1:1412");
    UpstreamManager::upstream_add_server("echo_server", "192.168.10.10");
    UpstreamManager::upstream_add_server("echo_server", "internal.host.com");

    // 2. 构造参数，填上upstream的名字
    RPCClientParams client_params = RPC_CLIENT_PARAMS_DEFAULT;
    client_params.host = "srpc::echo_server"; // 这个scheme只用于upstream URI解析
    client_params.port = 1412; // 这个port只用于upstream URI解析，不影响具体实例的选取

    // 3. 用参数创建client，其他用法与示例类似
    Example::SRPCClient client(&client_params);

    ...
```
