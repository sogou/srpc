## HTTP 

**srpc**支持**HTTP**协议，只要把**idl**的内容作填到**HTTP**的**body**中，并且在**header**里填上**idl**的类型(**json**/**protobuf**)，就可以与其他框架通过**HTTP**协议互通，由此可以实现跨语言。

- 启动**SRPCHttpServer**/**TRPCHttpServer**，可以接收由任何语言实现的HTTP client发过来的请求；

- 启动**SRPCHttpClient**/**TRPCHttpClient**，也可以向任何语言实现的Http Server发送请求；

- **HTTP header**：`Content-Type`设置为`application/json`表示json，`application/x-protobuf`表示protobuf；

- **HTTP body**： 如果body中涉及**bytes**类型，**json**中需要使用**base64**进行encode；

在项目的[README.md](/docs//README_cn.md#6-run)中，我们演示了如何使用**curl**向**SRPCHttpServer**发送请求，下面我们给出例子演示如何使用**python**作为客户端，向**TRPCHttpServer**发送请求。

### 示例

proto文件：

```proto
syntax="proto3"; // proto2 or proto3 are both supported

package trpc.test.helloworld;

message AddRequest {
    string message = 1;
    string name = 2;
    bytes info = 3;
};

message AddResponse {
    string message = 1;
};

service Batch {
    rpc Add(AddRequest) returns (AddResponse);
};
```

python客户端：

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
