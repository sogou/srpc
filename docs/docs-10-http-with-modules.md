
## 带生态插件的HttpServer和HttpClient

**srpc**提供带有插件功能的**HttpServer**和**HttpClient**，可以上报**trace**和**metrcis**，用法和功能完全兼容**Workflow**，添加插件的用法也和srpc目前的Server和Client一样，用于使用**Http功能**同时需**要采集trace、metrics等信息并上报**的场景。

此功能更加适用于原先已经使用Workflow收发Http的开发者，几乎不改动现有代码即可拥有生态上报功能。

(1) 补充插件基本介绍：

- **链路信息**：如何使用**trace**插件，并上报到[OpenTelemetry](https://opentelemetry.io)？ 参考：[docs-08-tracing.md](https://github.com/sogou/srpc/blob/master/docs/docs-08-tracing.md)
- **监控指标**：如何使用**metrics**插件，并且上报到[Prometheus](https://prometheus.io/)和[OpenTelemetry](https://opentelemetry.io)？参考：[docs-09-metrics.md](https://github.com/sogou/srpc/blob/master/docs/docs-09-metrics.md)

(2) 补充Workflow中的Http用法：
- [Http客户端任务：wget](https://github.com/sogou/workflow/blob/master/docs/tutorial-01-wget.md)
- [ttp服务器：http_echo_server](https://github.com/sogou/workflow/blob/master/docs/tutorial-04-http_echo_server.md)

(3) 补充SRPC中带插件上报功能的示例代码：

- [tutorial-17-http_server.cc](/tutorialtutorial-17-http_server.cc)
- [tutorial-18-http_client.cc](/tutorialtutorial-18-http_client.cc)


### 1. HttpServer用法

```cpp
int main()
{
    // 1. 构造方式与Workflow类似，Workflow用法是：
    //    WFHttpServer server([](WFHttpTask *task){ /* process */ });
    srpc::HttpServer server([](WFHttpTask *task){ /* process */ });

    // 2. 插件与SRPC通用，添加方式一致
    srpc::RPCTraceOpenTelemetry otel("http://127.0.0.1:4318");
    server.add_filter(&otel);

    // 3. server启动方式与Workflow/SRPC一样
    if (server.start(1412) == 0)
    {
        ...
    }
}
```

由于这里直接收发Http请求，因此有几个注意点：
1. 操作的是`WFHttpTask`（与Workflow中的一致），而不是由Protobuf或者Thrift等IDL定义的结构体；
2. 接口也不再是RPC里定义的**Service**了，因此也无需派生ServiceImpl实现RPC函数，而是直接给Server传一个`process函数`；
3. `process函数`格式也与Workflow一致：`std::function<void (WFHttpTask *task)>`；
4. process函数里拿到的参数是task，通过task->get_req()和task->get_resp()可以拿到请求与回复，分别是`HttpRequest`和`HttpResponse`，而其他上下文也在task而非**RPCContext**上；

### 2. HttpClient用法

```cpp
int main()
{
    // 1. 先构造一个client（与Workflow用法不同，与SRPC用法类似）
    srpc::HttpClient client;

    // 2. 插件与SRPC通用，添加方式一致
    srpc::RPCTraceDefault trace_log;
    client.add_filter(&trace_log);

    // 3. 发出Http请求的方式与Workflow类似，Workflow用法是：
    //    WFHttpTask *task = WFTaskFactory::create_http_task(...);
    //    函数名、参数、返回值与Workflow用法一致
    WFHttpTask *task = client.create_http_task("http://127.0.0.1:1412",
                                               REDIRECT_MAX,
                                               RETRY_MAX, 
                                               [](WFHttpTask *task){ /* callback */ });
    task->start();                                                                 
    ...                                                          
    return 0;
}
```

同样几个注意点：
1. 操作的是`WFHttpTask`（与Workflow中的一致），而不是由Protobuf或者Thrift等IDL定义的结构；
2. 如果想拿到req，可以`task->get_req()`，拿到的是`HttpRequest`，而不是XXXRequest之类；
3. 和请求相关的上下文也不再需要**RPCContext**了，都在task上；
4. callback函数格式也与Workflow一致：`std::function<void (WFHttpTask *task)>`；

### 3. Http协议采集的数据
   
**OpenTelemetry的trace数据官方文档**：  
https://opentelemetry.io/docs/reference/specification/trace/semantic_conventions/http/

SRPC框架的trace模块已经采集以下内容，并会通过各自的filter以不同的形式发出。

**1. 公共指标**

| 指标名 | 含义 | 类型 | 例子 | 备注 |
|-------|-----|-----|------|-----|
|srpc.state|  框架状态码  | int | 0 | |
|srpc.error|  框架错误码  | int | 1 |state!=0时才有 |
|http.status_code|  Http返回码  | string | 200 | state=0时才有 |
|http.method|  Http请求方法  | string | GET |  |
|http.scheme|  scheme  | string | https |  |
|http.request_content_length |  请求大小 | int | 3840 | state=0时才有 |
|http.response_content_length|  回复大小 | int | 141 | state=0时才有 |
|net.sock.family| 协议地址族 | string | inet |  |
|net.sock.peer.addr| 远程地址 | string | 10.xx.xx.xx | state=0时才有 |
|net.sock.peer.port| 远程端口 | int | 8080 | state=0时才有 |

**2. Client指标**

| 指标名 | 含义 | 类型 | 例子 | 备注 |
|-------|-----|-----|------|-----|
|srpc.error|  框架错误码  | int | 1 | state不为0时才有 |
|srpc.timeout_reason|  超时原因  | int | 2 | state=WFT_STATE_SYS_ERROR(1)和error=ETIMEDOUT(116)时才有 |
|http.resend_count|  框架重试次数  | int | 0 | state=0时才有 |
|net.peer.addr| uri请求的地址 | string | 10.xx.xx.xx | |
|net.peer.port| uri请求的端口 | int | 80 | |
|* http.url|  请求的url | string |  | |

state与error参考：[workflow/src/kernel/Communicator.h](workflow/src/kernel/Communicator.h)

timeout_reason参考：[workflow/src/kernel/CommRequest.h](workflow/src/kernel/CommRequest.h)

**3. Server指标**

| 指标名 | 含义 | 类型 | 例子 | 备注 |
|-------|-----|-----|------|-----|
|net.host.name|  服务器名称  | string | example.com | 虚拟主机名，来自比如对方header里发过来的Host信息等 | 
|net.host.port|  监听的端口  | int | 80 |  | 
|http.target| 完整的请求目标 | string | /users/12314/?q=ddds |  |
|http.client_ip| 原始client地址 | string | 10.x.x.x | 有时候会有，比如转发场景下，header中带有“X-Forwarded-For”等 |

**OpenTelemetry的metrics数据官方文档（SRPC正在支持中）**：   
https://opentelemetry.io/docs/reference/specification/metrics/semantic_conventions/http-metrics/

### 4. 示例数据

以下通过client-server的调用，我们可以看到这样的链路图：
```
[trace_id : 005028aa52fb0000005028aa52fb0002] [client]->[server]

timeline: 1681380123462593000   1681380123462895000   1681380123463045000   1681380123463213000

  [client][begin].........................................................................[end]
   span_id : 0400fb52aa285000
    
                                [server][begin].....................[end]
                                 span_id : 00305c54aa285000
                                 parent_span_id : 0400fb52aa285000
```

先通过`make tutorial`命令可以把tutorial里的`http_server`和`http_client`编出来，其中tutorial-17-http_server.cc中可以把上报OpenTelemetry的插件打开：

```cpp
srpc::RPCTraceOpenTelemetry otel("http://127.0.0.1:4318");
srpc::HttpServer server(process);
server.add_filter(&otel);
...
```

分别按照如下执行，可以看到我们的client通过RPCTraceDefault插件本地打印的trace信息：

```sh
./http_client
```
```
callback. state = 0 error = 0
<html>Hello from server!</html>
finish print body. body_len = 31
[SPAN_LOG] trace_id: 005028aa52fb0000005028aa52fb0002 span_id: 0400fb52aa285000 start_time: 1681380123462593000 finish_time: 1681380123463213000 duration: 620000(ns) http.method: GET http.request_content_length: 123145504128464 http.resend_count: 0 http.response_content_length: 31 http.scheme: http http.status_code: 200 net.peer.name: 127.0.0.1 net.peer.port: 1412 net.sock.family: inet net.sock.peer.addr: 127.0.0.1 net.sock.peer.port: 1412 component: srpc.srpc span.kind: srpc.client state: 0
^C
```
我们把server对OpenTelemetry上报的Protobuf内容也同时打印出来：

```sh
./http_server 
```
```sh
http server get request_uri: /
[SPAN_LOG] trace_id: 005028aa52fb0000005028aa52fb0002 span_id: 00305c54aa285000 parent_span_id: 0400fb52aa285000 start_time: 1681380123462895000 finish_time: 1681380123463045000 duration: 150000(ns) http.method: GET http.request_content_length: 0 http.response_content_length: 31 http.scheme: http http.status_code: 200 http.target: / net.host.name: 127.0.0.1:1412 net.host.port: 1412 net.sock.family: inet net.sock.peer.addr: 127.0.0.1 net.sock.peer.port: 56591 component: srpc.srpc span.kind: srpc.server state: 0

resource_spans {
  resource {
  }
  instrumentation_library_spans {
    spans {
      trace_id: "\000P(\252R\373\000\000\000P(\252R\373\000\002"
      span_id: "\0000\\T\252(P\000"
      parent_span_id: "\004\000\373R\252(P\000"
      name: "GET"
      kind: SPAN_KIND_SERVER
      start_time_unix_nano: 1681380123462895000
      end_time_unix_nano: 1681380123463045000
      attributes {
        key: "http.method"
        value {
          string_value: "GET"
        }
      }
      attributes {
        key: "http.request_content_length"
        value {
          int_value: 0
        }
      }
      attributes {
        key: "http.response_content_length"
        value {
          int_value: 31
        }
      }
      attributes {
        key: "http.scheme"
        value {
          string_value: "http"
        }
      }
      attributes {
        key: "http.status_code"
        value {
          int_value: 200
        }
      }
      attributes {
        key: "http.target"
        value {
          string_value: "/"
        }
      }
      attributes {
        key: "net.host.name"
        value {
          string_value: "127.0.0.1:1412"
        }
      }
      attributes {
        key: "net.host.port"
        value {
          int_value: 1412
        }
      }
      attributes {
        key: "net.sock.family"
        value {
          string_value: "inet"
        }
      }
      attributes {
        key: "net.sock.peer.addr"
        value {
          string_value: "127.0.0.1"
        }
      }
      attributes {
        key: "net.sock.peer.port"
        value {
          int_value: 56591
        }
      }
      status {
      }
    }
  }
}
```

可以看到用法和SRPC框架默认的trace模块是一样的。

### 5. 其他

补充一些常见问题，帮助开发中更好地理解这个模块。

**Q1: 本次新增的HttpServer与HttpClient，与SRPCHttpServer/SRPCHttpClient有什么共同点/不同点？**

共同点：
- 应用层协议相同，都是Http协议进行网络收发；
- 采集的数据依据相同，都是从Http Header收集和透传出去；

不同点：
- 用法不同：本次新增的Http模块是延续Workflow风格的用法，比如WFHttpTask；而SRPCHttp/ThriftHttp/TRPCHttp的用法是RPC模式，且包括了同步/异步/半同步的使用方式；
- 接口不同：前者用url直接定位要发的请求，而server也是一个process函数作为处理请求的统一入口；而原先的模块对Http只是网络层面的收发，url中的路由信息是通过${service}和${method}进行拼接的，然后把Protobuf或者Thrift这个结构体作为Http协议的body发出；
- 开发者接触的请求/回复不同：前者从task上拿出HttpRequest和HttpResponse，后者是Protobuf/Thrift里定义的Message；

**Q2: 和Workflow原生的Http协议是什么关系？**

srpc中新增的功能，开发者拿到的也是Workflow中定义的WFHttpTask，在实现上进行了行为派生，因此收发期间有几个切面可以进行一些模块化编程，这是和Workflow的Http相比更多的功能。

只要开发者把模块通过add_filter()加入到Server/Client中，通过产生的任务就是带有切面功能的，而服务开发者无需感知。

**Q3: 为什么放在SRPC项目中？**

目前生态插件所需要的几个功能都在SRPC项目中，包括：
- 收集数据的rpc_module，包括生成trace_id等通用功能；
- 上报信息的rpc_filter，包括使用Protobuf格式上报OpenTelemetry；
- 统计监控指标的rpc_var;
