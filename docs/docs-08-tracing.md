[English version](/docs/en/docs-08-tracing.md)

## 上报Tracing到OpenTelemetry
**SRPC**支持产生和上报链路信息trace和span，并且可以通过多种途径进行上报，其中包括本地导出数据和上报到[OpenTelemetry](https://opentelemetry.io).

**SRPC**遵循**OpenTelemetry**的[数据规范(data specification)](https://github.com/open-telemetry/opentelemetry-specification)以及[w3c的trace context](https://www.w3.org/TR/trace-context/)，因此可以使用插件**RPCTraceOpenTelemetry**进行上报。

秉承着**Workflow**的风格，所有的上报都是异步任务模式，对RPC的请求和服务不会产生任何性能影响。

### 1. 用法

先构造插件`RPCTraceOpenTelemetry`，然后通过`add_filter()`把插件加到**server**或**client**中。

以[tutorial-02-srpc_pb_client.cc](https://github.com/sogou/srpc/blob/master/tutorial/tutorial-02-srpc_pb_client.cc)作为client的示例，我们如下加两行代码：

```cpp
int main()
{
    Example::SRPCClient client("127.0.0.1", 1412);

    RPCTraceOpenTelemetry span_otel("http://127.0.0.1:4318");
    client.add_filter(&span_otel);

    ...
}
```

以[tutorial-01-srpc_pb_server.cc](https://github.com/sogou/srpc/blob/master/tutorial/tutorial-01-srpc_pb_server.cc)作为server的示例，也增加类似的两行。同时我们还增加一个本地插件，用于把本次请求的trace数据也在屏幕上打印：

```cpp
int main()
{
    SRPCServer server;

    RPCTraceOpenTelemetry span_otel("http://127.0.0.1:4318");
    server.add_filter(&span_otel);

    RPCTraceDefault span_log; // 这个插件会把本次请求的trace信息打到屏幕上
    server.add_filter(&span_log);

    ...
}
```

执行命令`make tutorial`可以编译出示例程序，并且分别把server和client跑起来，我们可以在屏幕上看到一些tracing信息：

<img width="1439" alt="image" src="https://user-images.githubusercontent.com/1880011/151154936-a84c1b9d-13b7-411f-938f-99cd2b001b89.png">

我们可以看到上图client这边的span_id: **04d070f537f17d00**，它在下图server这里变成了parent_span_id: **04d070f537f17d00**：

<img width="1439" alt="image" src="https://user-images.githubusercontent.com/1880011/151154895-4d0f81fd-dcf5-4b59-95a3-9a09f245fd51.png">

### 2. Traces上报到Jaeger

打开Jaeger的主页，我们可以找到我们名为**Example**的服务(service)和名为**Echo**的函数(method)。这一个tracing记录上有两个span节点，是由server和client分别上报的。

<img width="1438" alt="image" src="https://user-images.githubusercontent.com/1880011/151155282-38b09409-1700-4e76-a993-da10ee7dcdc5.png">

我们可以在Jaeger看到client所上报的span_id: **04d070f537f17d00**和server所上报的span_id: **00202cf737f17d00**，同时也是能对应上刚才在屏幕看到的id的。

<img width="1439" alt="image" src="https://user-images.githubusercontent.com/1880011/151155350-ac7123de-cc88-49c5-aac2-fe003fbe1d0b.png">

### 3. 参数

多久收集一份trace信息、上报请求的重试次数、以及其他参数，都可以通过`RPCTraceOpenTelemetry`的构造函数指定。代码参考：[src/module/rpc_trace_filter.h](https://github.com/sogou/srpc/blob/master/src/module/rpc_trace_filter.h#L238)

默认每秒收集1000条trace信息，并且透传tracing信息等其他功能也已遵循上述规范实现。

### 4. Attributes
我们可以通过`add_attributes()`添加某些额外的信息，比如数据规范中的OTEL_RESOURCE_ATTRIBUTES。

注意我们的service名"Example"也是设置到attributes中的，key为`service.name`。如果用户也在OTEL_RESOURCE_ATTRIBUTES中使用了`service.name`这个key，则SRPC的service name优先级更高，参考：[OpenTelemetry#resource](https://github.com/open-telemetry/opentelemetry-dotnet/tree/main/src/OpenTelemetry#resource)

### 5. Log和Baggage
SRPC提供了`log()`和`baggage()`接口，用户可以添加需要通过链路透传的数据。

```cpp
void log(const RPCLogVector& fields);
void baggage(const std::string& key, const std::string& value);
```

作为Server，我们可以使用`RPCContext`上的接口来添加log：

```cpp
class ExampleServiceImpl : public Example::Service                                 
{
public: 
    void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
    {
        resp->set_message("Hi back");
        ctx->log({{"event", "info"}, {"message", "rpc server echo() end."}});
    }
};
```

作为client，我们可以使用`RPCClientTask`上的接口添加log：

```cpp
srpc::SRPCClientTask *task = client.create_Echo_task(...);
task->log({{"event", "info"}, {"message", "log by rpc client echo()."}});
```
