[English version](README_en.md)

# srpc小工具

### 一个帮你快速生成Workflow和SRPC项目的脚手架小工具。

## 1. 编译

先从github上把srpc项目拷贝下来，小工具代码在srpc/tools/目录下，执行`make`即可编译。

```
git clone https://github.com/sogou/srpc
cd srpc/tools
make
```

## 2. 用法

执行`./srpc`即可看到小工具的用法介绍：

```
./srpc
```

```
Description:
    Simple generator for building Workflow and SRPC projects.

Usage:
    ./srpc <COMMAND> <PROJECT_NAME> [FLAGS]

Available Commands:
    http    - create project with both client and server
    redis   - create project with both client and server
    rpc     - create project with both client and server
    proxy   - create proxy for some client and server protocol
    file    - create project with asynchronous file service
    compute - create project with asynchronous computing service
```

## 3. 入门

我们先从最简单的命令开始入门：

```sh
./srpc http project_name1
```

然后就可以看到屏幕上显示，项目建立在了新目录`./project_name1/`中，并且带有编译和执行的提示命令。

```
Success:
      make project path " ./project_name1/ " done.

Commands:
      cd ./project_name1/
      make -j

Execute:
      ./server
      ./client
```

打开目录，我们查看一下有什么文件：

```
cd ./project_name1/ && tree
```

```
.
├── CMakeLists.txt
├── GNUmakefile
├── client.conf
├── client_main.cc
├── config
│   ├── Json.cc
│   ├── Json.h
│   ├── config.cc
│   ├── config.h
│   └── util.h
├── full.conf
├── server.conf
└── server_main.cc

2 directories, 12 files
```

然后我们就可以根据上面执行`srpc`命令时所看到的指引，编译和执行这个项目。

## 4. HTTP

创建HTTP项目的用法如下，可以创建http协议的server和client。其中server和client里的示例代码都可以自行改动，配置文件`server.conf`和`client.conf`里也可以指定基本的配置项，cmake编译文件都已经生成好了，整个项目可以直接拿走使用。

```
./srpc http
```

```
Missing: PROJECT_NAME

Usage:
    ./srpc http <PROJECT_NAME> [FLAGS]

Available Flags:
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

## 5. RPC

创建RPC项目的用法如下，包括了多种rpc协议，protobuf或thrift的文件：

```
./srpc rpc
```

```
Missing: PROJECT_NAME

Usage:
    ./srpc rpc <PROJECT_NAME> [FLAGS]

Available Flags:
    -r :    rpc type [ SRPC | SRPCHttp | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: SRPC)
    -o :    project output path (default: CURRENT_PATH)
    -s :    service name (default: PROJECT_NAME)
    -i :    idl type [ protobuf | thrift ] (default: protobuf)
    -x :    data type [ protobuf | thrift | json ] (default: idl type. json for http)
    -c :    compress type [ gzip | zlib | snappy | lz4 ] (default: no compression)
    -d :    path of dependencies (default: COMPILE_PATH)
    -f :    specify the idl_file to generate codes (default: template/rpc/IDL_FILE)
    -p :    specify the path for idl_file to depend (default: template/rpc/)
```

我们通过以下命令，试一下指定要创建的RPC项目所依赖的proto文件：

```
./srpc rpc rpc_example -f test_proto/test.proto 
```

然后就可以看到一些生成代码多信息，这和原先使用`srpc_genrator`时所看到的是类似的。

```
Info: srpc generator begin.
proto file: [/root/srpc/tools/rpc_example/test.proto]
Successfully parse service block [message] : EchoRequest
Successfully parse service block [message] : EchoResponse
Successfully parse service block [service] : new_test
Successfully parse method:Echo req:EchoRequest resp:EchoResponse
finish parsing proto file: [/root/srpc/tools/rpc_example/test.proto]
[Generator] generate srpc files: /root/srpc/tools/rpc_example/test.srpc.h 
[Generator Done]
[Generator] generate server files: /root/srpc/tools/rpc_example/server_main.cc, client files: /root/srpc/tools/rpc_example/client_main.cc
Info: srpc generator done.

Success:
      make project path " /root/srpc/tools/rpc_example/ " done.

Commands:
      cd /root/srpc/tools/rpc_example/
      make -j

Execute:
      ./server
      ./client
```

## 6. REDIS

创建REDIS协议的client和server，命令如下：

```
./srpc redis
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc redis <PROJECT_NAME> [FLAGS]

Available Flags:
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

根据以上指引我们创建了一个项目后，就可以得到最简单的redis server和client。client就简单地实现了发送`SET k1 v1`命令，而server无论收到什么都会简单地回复一个`OK`。我们可以用这简单的示例，改造一个可以请求任何redis协议服务的client，也可以构造一个简单的redis服务器。

```
./server

Redis server start, port 6379
redis server get cmd: [SET] from peer address: 127.0.0.1:60665, seq: 0.
```

```
./client

Redis client state = 0 error = 0
response: OK
```

如果client有填写用户名和密码的需求，可以填到`client.conf`中。我们打开这个配置文件看看：

```
  1 {                                                                               
  2   "client":                                                                     
  3   {                                                                             
  4     "remote_host": "127.0.0.1",                                                 
  5     "remote_port": 6379,                                                        
  6     "retry_max": 2,                                                             
  7     "user_name": "root",                                                        
  8     "password": ""                                                              
  9   }                                                                             
 10 }
```

## 7. PROXY

这个命令用于构建一个转发服务器，并且还有与其协议相关的server和client。

```
./srpc proxy
```

执行上述命令，我们可以看到proxy命令的指引：

```
Missing: PROJECT_NAME

Usage:
    ./srpc proxy <PROJECT_NAME> [FLAGS]

Available Flags:
    -c :    client type for proxy [ Http | Redis | SRPC | SRPCHttp | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)
    -s :    server type for proxy [ Http | Redis | SRPC | SRPCHttp | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

让我们来试一下，创建一个转发不同协议的项目：

```
./srpc proxy srpc_trpc_proxy_example -c SRPC -s TRPC
```
```
Success:
      make project path " ./srpc_trpc_proxy_example/ " done.

Commands:
      cd ./srpc_trpc_proxy_example/
      make -j

Execute:
      ./server
      ./proxy
      ./client
```
查看新创建的项目中有什么文件：

```
cd srpc_trpc_proxy_example && tree
```

```
.
├── CMakeLists.txt
├── GNUmakefile
├── client.conf
├── client_main.cc
├── config
│   ├── Json.cc
│   ├── Json.h
│   ├── config.cc
│   └── config.h
├── proxy.conf
├── proxy_main.cc
├── server.conf
├── server_main.cc
└── srpc_trpc_proxy_example.proto

2 directories, 13 files
```

分别在三个终端执行`./server` `./proxy` 和 `./client`，我们可以看到，client发送了trpc协议的请求"Hello, this is sync request!" 和一个异步请求Hello, this is async request!"给proxy，而proxy收到之后把请求用srpc协议发给了server。SRPC server填了回复"Hi back"并通过刚才的proxy路线转回给了client，期间转发纯异步，不会阻塞任何线程。

```
./server

srpc_trpc_proxy_example TRPC server start, port 1412
get req: message: "Hello, this is sync request!"
get req. message: "Hello, this is async request!"
```

```
./proxy

srpc_trpc_proxy_example [SRPC]-[TRPC] proxy start, port 1411
srpc_trpc_proxy_example proxy get request from client. ip : 127.0.0.1
message: "Hello, this is sync request!"
srpc_trpc_proxy_example proxy get request from client. ip : 127.0.0.1
message: "Hello, this is async request!"
```

```
./client

sync resp. message: "Hi back"
async resp. message: "Hi back"
```

## 8. FILE

这是一个简单的文件服务器，文件读取都是异步的，不会因为读文件阻塞当前服务器的处理线程：

```
./srpc file file_project
```

```
Success:
      make project path " ./file_project/ " done.

Commands:
      cd ./file_project/
      make -j

Execute:
      ./server

Try file service:
      curl localhost:8080/index.html
      curl -i localhost:8080/a/b/
```
我们通过上述命令创建之后，可以看看文件服务器的目录结构如下：

```
.
├── CMakeLists.txt
├── GNUmakefile
├── config
│   ├── Json.cc
│   ├── Json.h
│   ├── config.cc
│   └── config.h
├── file_service.cc
├── file_service.h
├── html
│   ├── 404.html
│   ├── 50x.html
│   └── index.html
├── server.conf
└── server_main.cc

3 directories, 13 files
```
打开`server.conf`，就可以看到我们为文件服务器添加的具体配置项：`root`和`error_page`。我们可以通过root去指定打开文件的根目录，以及通过error_page去关联具体的错误码和它们所要返回作body的页面名称。

```
  1 {
  2   "server":
  3   {
  4     "port": 8080,
  5     "root": "./html/",
  6     "error_page" : [
  7       {
  8         "error" : [ 404 ],
  9         "page" : "404.html"
 10       },
 11       {
 12         "error" : [ 500, 502, 503, 504],
 13         "page" : "50x.html"
 14       }
 15     ]
 16   }
 17 }
```

我们执行`make`进行编译，然后执行`./server`把文件服务器跑起来，然后用`curl`进行测试：

示例1：在根目录`./html/`下读取文件`index.html`（即使请求localhost:8080，默认也是读index.html）。
```
curl localhost:8080/index.html

<html>Hello from workflow and srpc file server!</html>
```

示例2：读文件`/a/b/`，这个文件不存在，所以我们根据上面配置文件`server.conf`中所指定的，填入`404`错误码会返回页面`404.html`的内容。

```
curl -i localhost:8080/a/b/
HTTP/1.1 404 Not Found
Server: SRPC HTTP File Server
Content-Length: 59
Connection: Keep-Alive

<html>This is not the web page you are looking for.</html>
```

以下信息在server端可以看到：

```
./server 
http file service start, port 8080
file service get request: /a/b/
```

## 9. COMPUTE

接下来是一个简单的计算服务器，同理，计算也不会阻塞当前服务器的处理线程：

```
./srpc compute compute_test
```

通过以上命令，我们可以创建一个小项目，项目默认接收url请求作为参数n，并进行斐波那契计算。

```
Success:
      make project path " compute_test/ " done.

Commands:
      cd compute_test/
      make -j

Execute:
      ./server

Try compute with n=8:
      curl localhost:8080/8
```

进入`compute_test/`目录执行`make`，并执行`./server`运行起来。然后我们就可以使用curl或者浏览器输入`localhost:8080/8`，计算第8个斐波那契数是多少。这里以`curl`为例：

```
curl localhost:8080/8

<html><p>0 + 1 = 1.</p><p>1 + 1 = 2.</p><p>1 + 2 = 3.</p><p>2 + 3 = 5.</p><p>3 + 5 = 8.</p><p>5 + 8 = 13.</p><p>The No. 8 Fibonacci number is: 13.</p></html>
```

我们看到了server内部的计算步骤。server的计算例子使用了go_task去封装一个计算函数，欢迎尝试更多的计算调度。

