[中文版](/docs/docs-09-metrics.md)

## Report Metrics to OpenTelemetry / Prometheus

**Metrics** are common monitoring requirements. **SRPC** supports the generation and statistics of Metrics, and reports through various way, including reporting to [Prometheus](https://prometheus.io/) and [OpenTelemetry](https://opentelemetry.io).

The report conforms to the **Workflow** style, which is pure asynchronous task or pull mode, and therefore has no performance impact on the RPC requests and services.

This document will introduce the concept of Metrics, explain the interface of [tutorial-16](/tutorial/tutorial-16-server_with_metrics.cc),  the usage of reporting to Prometheus, the usage of reporting to OpenTelemetry, and introduce the Var module which uses thread local to speed up performance.

### 1. Introduction

The metrics type of **Prometheus** can be refered through the official documentation : [Concepts - Metrics](https://prometheus.io/docs/concepts/metric_types/) and [Type of Metrics](https://prometheus.io/docs/tutorials/understanding_metric_types/)。

The basic metrics concept of **OpenTelemetry** can be refer through [(data specification)](https://github.com/open-telemetry/opentelemetry-specification) and [Metrics的datamodel.md](https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/metrics/datamodel.md).

The concepts of the four basic metrics are the same, so SRPC provides the most basic support for these four metrics, which can correspond to the reported data of Prometheus and OpenTelemetry respectively. Please refer to the table below:

|Metrics type|Prometheus|OpenTelemetry| create api in SRPC  |  some operations in SRPC |
|-------|----------|------------|-----------------|-------------------|
|single value|  Gauge   |    Gauge   | create_gauge(name, help); | void increase(); void decrease();|
|counter|  Counter |    Sum     | create_counter(name, help); |GaugeVar *add(labels);|
|histogram  |Histogram | Histogram  | create_histogram(name, help, buckets); |void observe(data);|
|samples    |  Summary |  Summary   | create_summary(name, help, quantiles); |void observe(data);|
   
Basic description of the four types of metrics:  
1. **Single value**: A simple value that can be increased or decreased.  
2. **Counter**: A counter is a cumulative metrics, which can be added several **labels** to distinguish the values under different labels.  
3. **Histogram**: A histogram samples observations (usually things like request durations or response sizes) and counts them in configurable buckets, so it is necessary to pass in **buckets** to inform the interval to be divided.  
  For example, if the input bucket value is { 1, 10, 100 }, then we can get the data distributed in these four intervals { 0 - 1, 0 - 10, 0 - 100, 0 - +Inf }. It also provides a sum and the total count of all observed values.  
4. **Samples**: Similar to histogram, but **quantiles** are passed in, such as {0.5, 0.9} and their precision. It mainly used in scenarios where the specific value of the data distribution is not known in advance (In contrast, histograms require specific values to be passed in).
  Sampling comes with a **time window**. You may specify the statistics window by **max_age** and the number of buckets by **age_bucket**. The default in SRPC is using 5 bucket in 60 seconds to switch times window.
  Considering the statistical complexity of **sampling Summary**, **Histogram** is recommended in preference instead.

For convenience, these four type of metrics in SRPC are of type **double**.

### 2. Usage

The example shows usage via [tutorial-16-server_with_metrics.cc](/tutorial/tutorial-16-server_with_metrics.cc). Althought this example is for the server, the usage on the client is the same.

#### (1) create filter

Here we use Prometheus to be reported, thus we need the filter **RPCMetricsPull** as the plugin.

~~~cpp
#include "srpc/rpc_filter_metrics.h" // metrics filter header file

int main()
{
    SRPCServer server;
    ExampleServiceImpl impl;
    RPCMetricsPull filter; // create a filter

    filter.init(8080); // the port for Prometheus to pull metrics data

~~~

#### (2) add metrics

By default, there are some common metrics in the filter **RPCMetricsPull**, including the number of overall request, the number of requests in different service or method as labels, and the quantiles of latency.

Users can add any metrics they want. Here we add a histogram namee "echo_request_size" to count the size of requests. This metric describes the information as "Echo request size", and the data bucket division is { 1, 10, 100 }.

~~~cpp
    filter.create_histogram("echo_request_size", "Echo request size", {1, 10, 100});
~~~

More illutrations:

**Timing to add metrics**

Metrics can be added any time, even after the server/client has been running. But **must be added before we use** this metrics, otherwise a NULL pointer will be obtained when we acquire the metrics by name.

**The name of the metrics**

The name of metrics is **globally unique** (no matter which of the four basic types), and **can only contain uppercase and lowercase letters and underscores, ie a~z, A~Z, _** . The creation will fail and return NULL if we create with an existed metric‘s name.

We will use this unique name to operate the metrics after it has been created.

**Other APIs to create metrics**

You can check this header file [rpc_filter_metrics.h](/src/module/rpc_filter_metrics.h): `class RPCMetricsFilter`.

#### (3) add filter into server / client

The following example keeps the pointer of this filter in ExampleServiceImpl because we may need the APIs on it to operate our metrics. It`s just one of the sample usages and users may use it in the way you used to.

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
	void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override；
	void set_filter(RPCMetricsPull *filter) { this->filter = filter; }

private:
	RPCMetricsPull *filter; // keep the pointer of filter and add set_filter(), notice that it's not APIs of SRPC
};
~~~

The usage in main:

~~~cpp
int main()
{
    ...
    impl.set_filter(&filter); // keep the filter throught the APIs in service we mentioned above

    server.add_filter(&filter); // client can also call add_filter()
    server.add_service(&impl);

    filter.deinit();
    return 0;
}
~~~

#### (4) operation with the metrics

Every time we receive a request, we get cumulative calculation of the size of the EchoRequest with the histogram we just created.

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

As has been shown, we use the API `histogram()` on filter to get the histogram metrics by it's name. Then we use `observe()` to fill in the data size.

Let's take a look at the APIs to acquire four types of metrics.

~~~cpp
class RpcMetricsFilter : public RPCFilter
{
    GaugeVar *gauge(const std::string& name);                                   
    CounterVar *counter(const std::string& name);                               
    HistogramVar *histogram(const std::string& name);                           
    SummaryVar *summary(const std::string& name);
~~~

If the metric is found by this name, the relevant type of pointer will be returned and we may use it to do the relevant next step, such as calling the observe() of historgram we did above. **Notice : If the metric name does not exist, Null will be returned, so we must ensure that the metrics we want must have be successfully created.**

Common APIs can be refered to the table above. For more details please refer to [rpc_var.h](/src/var/rpc_var.h).

It's worth taking a look at the API on Counter:

~~~cpp
class CounterVar : public RPCVar
{
    GaugeVar *add(const std::map<std::string, std::string>& labels);
~~~
This can add the Gauge value of a certain dimension  into this Counter. The statistics of each dimension are separated. The labels are a map, that is, a dimension can be specified through multiple groups of labels, for example:
~~~cpp
    filter->counter("service_method_count")->add({"service", "Example"}, {"method", "Echo"}})->increase();
~~~
Adn we can get the statistics calculated by `{service="Example",method="Echo"}`.

#### (5) Reporting

Reporting in SRPC filters is automatic, so users don't need to do anything. Next we will use a client to make some requests and check the format of data which will be reported.

~~~sh
./srpc_pb_client 

message: "Hi back"

message: "Hi back"
~~~

Since Prometheus will use Pull mode, which means will pull through the PORT we set before and the URL path as /metrics. When pulling with the same way as Prometheus do, we can get these with a simple local request.

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

It can be seen that we obtained four types of metrics by sending 2 requests with tutorial-02 client, among which the histogram was created by us and gauge, counter and summary were created by filter. The default metrics will be increased for the convinience of users.


### 3. Features of reporting to Prometheus

The main features of reporting to Prometheus have just been described:

1. Use the Pull mode and will collected regularly;
2. PORT is required to be pull;
3. Return data content through /metrics;
4. The data content is in the string format defined by Prometheus;

The interface reference is as follows:

![prometheus_img](https://raw.githubusercontent.com/wiki/holmes1412/holmes1412/workflow-prometheus_example.png)

### 4. Features of reporting OpenTelemetry

The main features of reporting to OpenTelemetry:

1. Use the push mode and will send http requests regularly;
2. URL is required to make report;
3. Reports data through /v1/metrics;
4. The data content is [protobuf] (/src/module/proto/opentelemetry_metrics.proto) as agreed by OpenTelemetry;

Basic APIs references:

~~~cpp
class RPCMetricsOTel : public RPCMetricsFilter
{
public:
    RPCMetricsOTel(const std::string& url);
    RPCMetricsOTel(const std::string& url, unsigned int redirect_max,           
                   unsigned int retry_max, size_t report_threshold,
                   size_t report_interval); 
~~~

The time interval to make report and the numbers of requests to make a report can be specified. By default, the filter will accumulate 100 requests or report once every second.


### 5. Var

Would it be the bottle neck of performance when every request use the globally unique name to get the metrics, especially multi-threaded operations with the complicated metrics (histogram or summary)?

The answer is NEVER. Because the APIs for getting metrics through filters are **thread-local**.

SRPC builds a thread-local var system. Every time we get a var, we get its thread-local var pointer. Therefore every time we calculate will only be collected by the current thread. That's why multiple threads will not be conflicted by global mutex. Moreover, since reporting is always asynchronous, a global API expose() will get all the vars in all the threads and reduce them together when reports. Lastly the data will be reported in the format with specific filter.
