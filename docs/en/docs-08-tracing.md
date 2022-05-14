[中文版](/docs/tutorial-08-tracing.md)

## Report Tracing to OpenTelemetry
**SRPC** supports generating and reporting tracing and spans, which can be reported in multiple ways, including exporting data locally or to [OpenTelemetry](https://opentelemetry.io).

Since **SRPC** follows the [data specification](https://github.com/open-telemetry/opentelemetry-specification) of **OpenTelemetry** and the specification of [w3c trace context](https://www.w3.org/TR/trace-context/), now we can use **RPCSpanOpenTelemetry** as the reporting plugin.

The report conforms to the **Workflow** style, which is pure asynchronous task and therefore has no performance impact on the RPC requests and services.

### 1. Usage

After the plugin `RPCSpanOpenTelemetry` is constructed, we can use `add_filter()` to add it into **server** or **client**.

For [tutorial-02-srpc_pb_client.cc](https://github.com/sogou/srpc/blob/master/tutorial/tutorial-02-srpc_pb_client.cc), add 2 lines like the following : 
```cpp
int main()                                                                   
{                                                                        
    Example::SRPCClient client("127.0.0.1", 1412); 

    RPCSpanOpenTelemetry span_otel("http://127.0.0.1:55358"); // jaeger http collector ip:port   
    client.add_filter(&span_otel);
    ...
```

For [tutorial-01-srpc_pb_server.cc](https://github.com/sogou/srpc/blob/master/tutorial/tutorial-01-srpc_pb_server.cc), add the similar 2 lines. We also add the local plugin to print the reported data on the screen :

```cpp
int main()
{
    SRPCServer server;  

    RPCSpanOpenTelemetry span_otel("http://127.0.0.1:55358");                            
    server.add_filter(&span_otel);                                                 

    RPCSpanDefault span_log; // this plugin will print the tracing info on the screen                                                  
    server.add_filter(&span_log);                                              
    ...
```

make the tutorial and run both server and client, we can see some tracing information on the screen. 

<img width="1439" alt="image" src="https://user-images.githubusercontent.com/1880011/151154936-a84c1b9d-13b7-411f-938f-99cd2b001b89.png">

We can find the span_id: **04d070f537f17d00** in client become parent_span_id: **04d070f537f17d00** in server:

<img width="1439" alt="image" src="https://user-images.githubusercontent.com/1880011/151154895-4d0f81fd-dcf5-4b59-95a3-9a09f245fd51.png">

### 2. Traces on Jaeger

Open the show page of Jaeger, we can find our service name **Example** and method name **Echo**. Here are two span nodes, which were reported by server and client respectively.

<img width="1438" alt="image" src="https://user-images.githubusercontent.com/1880011/151155282-38b09409-1700-4e76-a993-da10ee7dcdc5.png">

As what we saw on the screen, the client reported span_id: **04d070f537f17d00** and server reported span_id: **00202cf737f17d00**, these span and the correlated tracing information can be found on Jaeger, too.

<img width="1439" alt="image" src="https://user-images.githubusercontent.com/1880011/151155350-ac7123de-cc88-49c5-aac2-fe003fbe1d0b.png">

### 3. About Parameters

How long to collect a trace, and the number of reported retries and other parameters can be specified through the constructor parameters of `RPCSpanOpenTelemetry`. Code reference: [src/module/rpc_filter_span.h](https://github.com/sogou/srpc/blob/master/src/module/rpc_filter_span.h#L238)

The default value is to collect up to 1000 trace information per second, and features such as transferring tracing information through the srpc framework transparently have also been implemented, which also conform to the specifications. 

### 4. Attributes
We can also use `add_attributes()` to add some other informations as OTEL_RESOURCE_ATTRIBUTES.

Please notice that our service name "Example" is set also thought this attributes, the key of which is `service.name`. If `service.name` is also provided in OTEL_RESOURCE_ATTRIBUTES by users, then srpc service name takes precedence. Refers to : [OpenTelemetry#resource](https://github.com/open-telemetry/opentelemetry-dotnet/tree/main/src/OpenTelemetry#resource)

### 5. Log and Baggage
SRPC provides `log()` and `baggage()` to carry some user data through span.

API :

```cpp
void log(const RPCLogVector& fields);
void baggage(const std::string& key, const std::string& value);
```

As a server, we can use `RPCContext` to add log annotation:

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

As a client, we can use `RPCClientTask` to add log on span:

```cpp
srpc::SRPCClientTask *task = client.create_Echo_task(...);
task->log({{"event", "info"}, {"message", "log by rpc client echo()."}});
```
