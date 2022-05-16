## 上报Metrics


**Metrics**(指标)是常用的监控需求，**SRPC**支持产生与统计Metrics，并通过多种途径上报，其中包括上报到[Prometheus](https://prometheus.io/)和[OpenTelemetry](https://opentelemetry.io)。

秉承着**Workflow**的风格，所有的上报都是异步任务推送或拉取模式，对RPC的请求和服务不会产生任何性能影响。

本文档会介绍Metrics的概念、结合tutorial-16对接口进行讲解、上报Prometheus的特点、上报OpenTelemetry的特点、以及介绍使用了thread local进行性能提速的Var模块。

### 1. Metrics概念介绍

**Prometheus**的数据类型可以参考官方文档：[Concepts - Metrics](https://prometheus.io/docs/concepts/metric_types/) 和 [Type of Metrics](https://prometheus.io/docs/tutorials/understanding_metric_types/)。

**OpenTelemetry**的可以参考[数据规范(data specification)](https://github.com/open-telemetry/opentelemetry-specification)以及[Metrics的datamodel.md](https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/metrics/datamodel.md)。

其中四种基本指标的概念都是一致的，因此SRPC对这四种指标进行了最基本的支持，可分别对应到Prometheus和OpenTelemetry的上报数据中。参考下表：

|指标类型|Prometheus|OpenTelemetry| SRPC中的创建接口  |   SRPC中的常用操作 |
|-------|----------|------------|-----------------|-------------------|
|单个数值|  Gauge   |    Gauge   | create_gauge(name, help); | void increase(); void decrease();|
|计数器  |  Counter |    Sum     | create_counter(name, help); |GaugeVar *add(labels);|
|直方图  |Histogram | Histogram  | create_histogram(name, help, buckets); |void observe(data);|
|采样    |  Summary |  Summary   | create_summary(name, help, quantiles); |void observe(data);|

四种指标的大致描述：
1. **单个数值**：简单数值的度量，可以简单的增减。
2. **计数器**：累计的度量，可以通过添加若干**label**去区分不同label下的数值。
3. **直方图**：对观察observer()得到的接口进行分区间累加，因此需要传入**buckets**告知需要划分的区间；   
   比如传入的bucket分桶值是{ 1, 10, 100 }这3个数，则我们可以得到数据分布于{ 0 - 1, 0 - 10, 0 - 100, 0 - +Inf }4个区间的数据累计值，同时也会得到整体的总以及数据个数。
4. **采样**：与直方图类似，但传入的是**分位数quantiles**，比如{0.5, 0.9}以及它们的精度，主要用于事先不知道数据分布具体数值的场景（相比之下，直方图需要传入具体数值）。   
   采样是带有**时间窗口**的，可以通过接口指定**max_age统计窗口时**长以及**age_bucket内部分桶个数**，SRPC中默认的是60秒分5个桶切换时间窗口。
   考虑到**采样Summary**本身的统计复杂性，一般来说建议优先使用**直方图Histogram**。

为了方便用户使用，目前SRPC里这四种指标都使用double类型进行统计。

### 2. 用法示例

我们结合[tutorial-16-server_with_metrics.cc](/tutorial/tutorial-16-server_with_metrics.cc)看看基本用法，本示例虽然是server，但client中的用法也是一样的。

#### (1) 创建插件

我们选择Prometheus作为我们的上报对象，因此需要使用**RPCMetricsPull**插件。

~~~cpp
#include "srpc/rpc_filter_metrics.h" // Metrics插件所在的头文件

int main()
{
    SRPCServer server;
    ExampleServiceImpl impl;
    RPCMetricsPull filter; // 创建一个插件

    filter.init(8080); // 配合Prometheus中填好的收集数据的端口

~~~

#### (2) 添加指标

这个**RPCMetricsPull**插件本身自带了统计部分常用指标，包括整体请求个数统计、按照service和method作为label的不同维度的请求个数统计以及请求耗时的分为数统计等。

用户可以自行增加想要统计的值，这里我们增加一个用于统计请求大小的直方图histogram，名字为"echo_request_size"，上报时的指标描述信息是"Echo request size"，数据bucket划分是 { 1, 10, 100 } 。

~~~cpp
    filter.create_histogram("echo_request_size", "Echo request size", {1, 10, 100});
~~~

说明： 

**添加指标的时机**   
指标是可以随时添加的，即使server/client跑起来之后也可以。但**必须在操作这个指标之前添加**，否则获取指标的时候会获取到空指针，无法进行统计操作。

**指标的名字**   
指标的名字是**全局唯一**的（无论是四种基本类型中的哪种），且**只能包含大小写字母和下划线，即a~z, A~Z, _** 。如果使用已经存在的名字创建指标，则会创建失败。   
我们之后会使用同一个名字去操作这个指标。

**创建其他指标的接口**   
可以查看刚才include的头文件[rpc_filter_metrics.h](/src/module/rpc_filter_metrics.h)中的：`class RPCMetricsFilter`。

#### (3) 把插件添加到server/client中

由于我们需要操作指标时，是需要调用这个插件上的接口的，因此我们在service中保留一下这个指针。这只是示例程序的用法，用户可以使用自己习惯的方式：

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
	void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override；
	void set_filter(RPCMetricsPull *filter) { this->filter = filter; }

private:
	RPCMetricsPull *filter; // 保留了插件的指针，并实现接口设置进去，这并非SRPC框架的接口
};
~~~

main函数中继续这样写：

~~~cpp
int main()
{
    ...
    impl.set_filter(&filter); // 设置到我们刚才为service留的接口中

    server.add_filter(&filter); // client也一样可以调用add_filter()
    server.add_service(&impl);

    filter.deinit();
    return 0;
}
~~~

#### (4) 操作指标

我们在每次收到请求的时候，都把EchoRequest的大小统计到刚才创建的直方图指标上：

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
	void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
	{
		resp->set_message("Hi back");
		this->filter->histogram("echo_request_size")->observe(req->ByteSizeLong());
	}
~~~

可以看到，我们通过filter上的histogram()接口，就可以带着刚才的名字去到指标的指针，并且通过observe()填入数据大小。

四种基本类型的获取接口如下：
~~~cpp
class RpcMetricsFilter : public RPCFilter
{
    GaugeVar *gauge(const std::string& name);                                   
    CounterVar *counter(const std::string& name);                               
    HistogramVar *histogram(const std::string& name);                           
    SummaryVar *summary(const std::string& name);
~~~
如果找到，会返回四种基本指标类型的指针，可以如示例进行下一步操作比如histogram的统计接口observe()，**注意：不存在这个名字则会返回空指针，因此要保证我们拿到的变量一定是成功创建过的**。

四种类型常用的操作接口如上文表格所示，具体可以参考[rpc_var.h](/src/var/rpc_var.h)。

值得说明一下Counter类型的接口：
~~~cpp
class CounterVar : public RPCVar
{
    GaugeVar *add(const std::map<std::string, std::string>& labels);
~~~
接口可以对Counter指标添加某个维度的Gauge值，各维度的统计是分开的，且labels为一个map，即可以通过多组label指定一个维度，比如：
~~~cpp
    filter->counter("service_method_count")->add({"service", "Example"}, {"method", "Echo"}})->increase();
~~~
就可以获得一个针对`{service="Example",method="Echo"}`统计出来的数值。

#### (5) 自动上报

SRPC的插件都是自动上报的，因此无需用户调用任何接口。我们尝试调用client发送请求产生一些统计数据，然后看看上报出来的数据是什么。

~~~sh
./srpc_pb_client 

message: "Hi back"

message: "Hi back"
~~~

由于Prometheus是使用Pull模式拉取，即会通过我们注册到Prometheus的端口和/metrics进行拉取，也就是我们刚才初始化上报插件需要配对上端口的原因。通过与Prometheus相同的方式，我们可以本地访问一下，会看到这样的一些数据：

~~~sh
curl localhost:8080/metrics

# HELP total_request_count total request count
# TYPE total_request_count gauge
total_request_count 2.000000
# HELP total_request_method request method statistics
# TYPE total_request_method counter
total_request_method{method="Echo",service="Example"} 2.000000
# HELP total_request_latency request latency nano seconds
# TYPE total_request_latency summary
total_request_latency{quantile="0.500000"} 645078.500000
total_request_latency{quantile="0.900000"} 645078.500000
total_request_latency_sum 1290157.000000
total_request_latency_count 2
# HELP echo_request_size Echo request size
# TYPE echo_request_size histogram
echo_request_size_bucket{le="1.000000"}0
echo_request_size_bucket{le="10.000000"}0
echo_request_size_bucket{le="100.000000"}2
echo_request_size_bucket{le="+Inf"} 2
echo_request_size_sum 40.000000
echo_request_size_count 2
~~~

可以看到，我们针对tutorial-02的client产生的两个请求，分别获得的四种基本指标的统计数据。其中histogram是我们创建的，而gauge、counter、summary都是插件中自带的。插件中默认统计的数据还会陆续添加，以方便开发者。

### 3. 上报Prometheus的特点

上报Prometheus主要特点刚才已经大概描述过：

1. 使用Pull模式，定期被收集；
2. 需要指定我们被收集数据的端口；
3. 通过/metrics返回具体数据内容；
4. 数据内容为Prometheus所约定的string格式；

界面参考如下：
![prometheus_img](https://raw.githubusercontent.com/wiki/holmes1412/holmes1412/workflow-prometheus_example.png)

### 4 上报OpenTelemetry的特点

上报OpenTelemetry的主要特点：

1. 使用推送模式，定期发送http请求；
2. 插件需要填入要上报的URL；
3. 内部默认通过/v1/metrics上报数据；
4. 数据内容为OpenTelemetry所约定的[protobuf](/src/module/proto/opentelemetry_metrics.proto)；

基本接口参考：
~~~cpp
class RPCMetricsOTel : public RPCMetricsFilter
{
public:
    RPCMetricsOTel(const std::string& url);
    RPCMetricsOTel(const std::string& url, unsigned int redirect_max,           
                   unsigned int retry_max, size_t report_threshold,
                   size_t report_interval); 
~~~

用户可以指定累计多少个请求上报一次或者累计多久上报一次，默认为累计100个请求或1秒上报一次。

### 5. Var模块

上面可以看到，我们每个请求都会对全局唯一名称指定的变量进行操作，那么多线程调用时，对复杂的指标类型（比如直方图或者采样）操作会成为性能瓶颈吗？

答案是不会的，因为通过filter获取对应指标的接口是**thread local**的。

SRPC内部引入了线程安全的var结构，每次获取var时调用的接口，拿到的都是thread local的指标指针，每次统计也都是分别收集到本线程，因此多线程情况下的统计不会造成对全局的争抢。而上报是异步上报的，在上报被触发的时候，全局会通过expose()挨个把每个线程中相应的指标reduce()到一起，最后通过具体模块需要的格式进行上报。

