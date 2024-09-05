[中文版](/docs/docs-07-srpc-http.md)

## 07 - Use Http with SRPC, TRPC and Thrift

**srpc** supports **HTTP** protocol, which make it convenient to communicate to other language. Just fill the **IDL** content into **HTTP body**, and fill the **IDL** type(**json**/**protobuf**/**thrift**) into **Http header** , we communicate with other frameworks through the **HTTP** protocol.

- **SRPCHttpServer**, **TRPCHttpServer** and **ThriftHttpServer** can receive HTTP requests from client implemented by any language.

- **SRPCHttpClient**, **TRPCHttpClient** and **ThriftHttpClient** can send HTTP requests to servers implemented by any language.

- **HTTP header**: `Content-Type` needs to be set as `application/json` when body is json, or set as `application/x-protobuf` when body is protobuf, or set as `application/x-thrift` when body is thrift;

- **HTTP body**: If there is **bytes** type in body, **json** needs to be encoded with **base64**.

### 1. Example

To implement **SRPCHttpClient** is so simple that we just need to refer to [tutorial-02-srpc_pb_client.cc](https://github.com/sogou/srpc/blob/master/tutorial/tutorial-02-srpc_pb_client.cc) or [tutorial-09-client_task.cc](https://github.com/sogou/srpc/blob/master/tutorial/tutorial-09-client_task.cc) and change the `SRPCClient` into `SRPCHttpClient`.

[README.md](/docs/en/README.md#6-run) demonstrates how to use **curl** to send a request to **SRPCHttpServer**, and below we give an example to demonstrate how to use **python** as a client to send a request to **TRPCHttpServer**.


**proto file :**

```proto
syntax="proto3"; // proto2 or proto3 are both supported

package trpc.test.helloworld;

message AddRequest {
    string message = 1;
    string name = 2;
    bytes info = 3;
    int32 error = 4;
};

message AddResponse {
    string message = 1;
};

service Batch {
    rpc Add(AddRequest) returns (AddResponse);
};
```

**python client :**

```py
import json
import requests
from base64 import b64encode

class Base64Encoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, bytes):
            return b64encode(o).decode()
        return json.JSONEncoder.default(self, o)

headers = {'Content-Type': 'application/json'}

req = {
    'message': 'hello',
    'name': 'k',
    'info': b'i am binary'
}

print(json.dumps(req, cls=Base64Encoder))

ret = requests.post(url = "http://localhost:8800/trpc.test.helloworld.Batch/Add",
                    headers = headers, data = json.dumps(req, cls=Base64Encoder))
print(ret.json())
```

### 2. Splicing the path of HTTP request

[README.md](/docs/en/README_cn.md#6-run) shows that the path is concatenated by the service name and the rpc name. Moreover, for the above proto file example with the package name `package trpc.test.helloworld;`, the package name also needs to be spliced into the path. The splicing paths of **SRPCHttp** and **TRPCHttpClient** are different. For **ThriftHttp**, multi service is not supported in SRPC thrift, so no path is required.

Let's take **curl** as an example:

Request to **SRPCHttpServer**:
```sh
curl 127.0.0.1:8811/trpc/test/helloworld/Batch/Add -H 'Content-Type: application/json' -d '{...}'
```

Request to **TRPCHttpServer**:
```sh
curl 127.0.0.1:8811/trpc.test.helloworld.Batch/Add -H 'Content-Type: application/json' -d '{...}'
```

Request to **ThriftHttpServer**:
```sh
curl 127.0.0.1:8811 -H 'Content-Type: application/json' -d '{...}'
```

### 3. HTTP status code

SRPC supports server to set HTTP status code in `process()`, the interface is `set_http_code(int code)` on **RPCContext**. This error code is only valid if the framework can handle the request correctly, otherwise it will be set to the SRPC framework-level error code.

**Usage :**

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
    void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
    {
        if (req->name() != "workflow")
            ctx->set_http_code(404); // set HTTP status code as 404
        else
            resp->set_message("Hi back");
    }
};
~~~

**CURL command :**

~~~sh
curl -i 127.0.0.1:1412/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
~~~

**Result :**

~~~sh
HTTP/1.1 404 Not Found
SRPC-Status: 1
SRPC-Error: 0
Content-Type: application/json
Content-Encoding: identity
Content-Length: 21
Connection: Keep-Alive
~~~

**Notice :**

We can still distinguish this request is correct at framework-level by `SRPC-Status: 1` in the header of the returned result, and `404` is the status code from the server.

### 4. HTTP Header

Use the following three interfaces to get or set HTTP header:

~~~cpp
bool get_http_header(const std::string& name, std::string& value) const;
bool set_http_header(const std::string& name, const std::string& value);
bool add_http_header(const std::string& name, const std::string& value);
~~~

For **server**, these interfaces are on `RPCContext`.
For **client**, we need to set the **HTTP header on the request** through `RPCClientTask`, and get the **HTTP header on the response** on the `RPCContext` of the callback function. The usage is as follows:

~~~cpp
int main()
{
    Example::SRPCHttpClient client("127.0.0.1", 80);
    EchoRequest req;
    req.set_message("Hello, srpc!");

    auto *task = client.create_Echo_task([](EchoResponse *resp, RPCContext *ctx) {                                                                              
        if (ctx->success())
        {
            std::string value;
            ctx->get_http_header("server_key", value); // get the HTTP header on response
        }
    });
    task->serialize_input(&req);
    task->set_http_header("client_key", "client_value"); // set the HTTP headers on request
    task->start();
	
    wait_group.wait();
    return 0;
}
~~~

### 5. proto3 transport format problem

When IDL is protobuf, by default proto3 primitive fields with default values will be omitted in JSON output because there is no **optional** and **required** distinction. For example, an int32 field set to 0 will be omitted.So proto3 cannot be recognized as being set, and it cannot be emitted when serialized. JsonPrintOptions can use a flag to override the default behavior and print primitive fields regardless of their values.

SRPC provides some interfaces on **RPCContext上** to implement this and other flags on [JsonPrintOptions](https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.util.json_util#JsonPrintOptions). The specific interface and usage description can be refered in:[rpc_context.h](/src/rpc_context.h)

**Exmaple :**

```cpp
class ExampleServiceImpl : public Example::Service
{
public:                                                                         
    void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
    {
        resp->set_message("Hi back");
        resp->set_error(0); // the type of error is int32 and 0 is the default value of int32
        ctx->set_json_always_print_fields_with_no_presence(true); // all fields with no precense
        ctx->set_json_add_whitespace(true); // add spaces, line breaks and indentation 
    }
};
```

**Origin output :**

```sh
{"message":"Hi back"}
```

**After set json options on RPCContext:**
```sh
{
 "message": "Hi back",
 "error": 0
}
```
