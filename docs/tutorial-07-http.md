## HTTP 

**srpc**支持**HTTP**协议，只要把**idl**的内容作填到**HTTP**的**body**中，并且在**header**里填上**idl**的类型(**json**/**protobuf**)，就可以与其他框架通过**HTTP**协议互通，由此可以实现跨语言。

- 启动**SRPCHttpServer**/**TRPCHttpServer**，可以接收由任何语言实现的HTTP client发过来的请求；

- 启动**SRPCHttpClient**/**TRPCHttpClient**，也可以向任何语言实现的Http Server发送请求；

- **HTTP header**：`Content-Type`设置为`application/json`表示json，`application/x-protobuf`表示protobuf；

- **HTTP body**： 如果body中涉及**bytes**类型，**json**中需要使用**base64**进行encode；

### 示例

在项目的[README.md](/docs//README_cn.md#6-run)中，我们演示了如何使用**curl**向**SRPCHttpServer**发送请求，下面我们给出例子演示如何使用**python**作为客户端，向**TRPCHttpServer**发送请求。

**proto文件：**

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

**python客户端：**

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

### 请求路径拼接

[README.md](/docs//README_cn.md#6-run)中，我们可以看到，路径是由service名和rpc名拼接而成的。而对于以上带package名 `package trpc.test.helloworld;`的例子， package名也需要拼接到路径中，**SRPCHttp** 和 **TRPCHttpClient** 的拼接路径方式并不一样。我们以**curl**为例子：

与**SRPCHttpServer**互通：
```sh
curl 127.0.0.1:8811/trpc/test/helloworld/Batch/Add -H 'Content-Type: application/json' -d '{...}'
```

与**TRPCHttpServer**互通：
```sh
curl 127.0.0.1:8811/trpc.test.helloworld.Batch/Add -H 'Content-Type: application/json' -d '{...}'
```

### HTTP状态码

SRPC支持server在`process()`中设置状态码，接口为**RPCContext**上的`set_http_code(int code)`。只有在框架能够正确处理请求的情况下，该错误码才有效，否则会被设置为框架层级的错误码。

**用法：**

~~~cpp
class ExampleServiceImpl : public Example::Service
{
public:
    void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
    {
        if (req->name() != "workflow")
            ctx->set_http_code(404); // 设置HTTP状态码404
        else
            resp->set_message("Hi back");
    }
};
~~~

**CURL命令：**

~~~sh
curl -i 127.0.0.1:1412/Example/Echo -H 'Content-Type: application/json' -d '{message:"from curl",name:"CURL"}'
~~~

**结果：**

~~~sh
HTTP/1.1 404 Not Found
SRPC-Status: 1
SRPC-Error: 0
Content-Type: application/json
Content-Encoding: identity
Content-Length: 21
Connection: Keep-Alive
~~~

**注意：**

我们依然可以通过返回结果的header中的`SRPC-Status: 1`来判断这个请求在框架层面是正确的，`404`是来自server的状态码。

### IDL传输格式问题

如果我们填写的是Protobuf且用的标准为proto3，每个域由于没有optional和required区分，所以都是带有默认值的。如果我们设置的值正好等于默认值，则proto3不能识别为被set过，就不能被序列化的时候发出。

在protobuf转json的过程中，SRPC在**RPCContext上**提供了几个接口，支持 [JsonPrintOptions](https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.util.json_util#JsonPrintOptions) 上的功能。具体接口与用法描述可以查看：[rpc_context.h](/src/rpc_context.h)

**示例：**

```cpp
class ExampleServiceImpl : public Example::Service
{
public:                                                                         
    void Echo(EchoRequest *req, EchoResponse *resp, RPCContext *ctx) override
    {
        resp->set_message("Hi back");
        resp->set_error(0); // 0是error类型int32在proto3中的默认值
        ctx->set_json_always_print_primitive_fields(true); // 带上所有原始域
        ctx->set_json_add_whitespace(true); // 增加json格式的空格
    }
};
```

**原始输出：**

```sh
{"message":"Hi back"}
```

**通过RPCContext设置过json options之后的输出：**
```sh
{
 "message": "Hi back",
 "error": 0
}
```
