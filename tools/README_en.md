[中文版入口](README.md)

# srpc tools

### Simple generator for building Workflow and SRPC projects.

## 1. COMPILE

```
git clone https://github.com/sogou/srpc
cd srpc/tools
make
```

## 2. USAGE

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

## 3. START

Execute this simple example:

```sh
./srpc http project_name1
```

And we will get this on the screen, new project is in `./project_name1/`.

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

Let's take a look at the project:

```
cd ./project_name1/ && tree
```

These files are generated.

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

And we can try to make the project accorrding to the suggestions above.

## 4. HTTP COMMAND

commands for HTTP:

```
./srpc http
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc http <PROJECT_NAME> [FLAGS]

Example:
    ./srpc redis my_http_project

Available Flags:
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

## 5. RPC COMMAND

commands for RPCs:

```
./srpc rpc
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc rpc <PROJECT_NAME> [FLAGS]

Example:
    ./srpc redis my_rpc_project

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

We can specified our IDL files with the following command:

```
./srpc rpc rpc_example -f test_proto/test.proto 
```

And we can see the infomation when generating files, which is similar to srpc_genrator.

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

## 6. API COMMAND

This command is to create an IDL file, which is convenient for us to create rpc project after changing the file. The command is as follows:

```
Missing: FILE_NAME

Usage:
    ./srpc api <FILE_NAME> [FLAGS]

Example:
    ./srpc api my_api

Available Flags:
    -o : file output path (default: CURRENT_PATH)
    -i : idl type [ protobuf | thrift ] (default: protobuf)
```

Create a file in proto format by default:

```
./srpc api test
```

```
Success:
      Create api file test.proto at path /root/srpc/tools done.

Suggestions:
      Modify the api file as you needed.
      And make rpc project base on this file with the following command:

      ./srpc rpc my_rpc_project -f test.proto -p /root/srpc/tools
```

Then we can create an rpc project through the above suggested commands. The rpc project uses the test.proto file to create the generated code.

## 7. REDIS COMMAND

commands for REDIS:

```
./srpc redis
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc redis <PROJECT_NAME> [FLAGS]

Example:
    ./srpc redis my_redis_project

Available Flags:
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

Make a project with the instructions, we can get the simple redis server and client. The client will send a basic command `SET k1 v1`, and the server will reply `OK` for every request.

```
./server

Redis server started, port 6379
redis server get cmd: [SET] from peer address: 127.0.0.1:60665, seq: 0.
```

```
./client

Redis client state = 0 error = 0
response: OK
```

If there is user name and password for redis server, client may fill them into client.conf:

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

## 8. PROXY COMMAND

commands for PROXY:

```
./srpc proxy
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc proxy <PROJECT_NAME> [FLAGS]

Example:
    ./srpc redis my_proxy_project

Available Flags:
    -c :    client type for proxy [ Http | Redis | SRPC | SRPCHttp | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)
    -s :    server type for proxy [ Http | Redis | SRPC | SRPCHttp | BRPC | Thrift | ThriftHttp | TRPC | TRPCHttp ] (default: Http)
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

Let's make a project with diffrent protocol:

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

Check the files in directory:

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

Execute `./server` `./proxy` and `./client` on the three sessions respectively, and we will see that the client sends trpc protocol requests "Hello, this is sync request!" and "Hello, this is async request!" to the proxy, and the proxy receives the request and send to server as srpc protocol. SRPC server fill the response "Hi back" and will finnally transfer back to client by proxy. 

```
./server

srpc_trpc_proxy_example TRPC server started, port 1412
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

## 9. FILE COMMAND

This is a simple file server. File reading is asynchronous, and the  threads for process( )  will not be blocked by file IOs.

```
./srpc file file_project
```

will get the following infomation:

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

Check the files in directories:

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

Also check the `server.conf`, we can see the `root` and `error_page` are added into file service. We can specify the root to find files for this service, and fill the page coresponding to the error codes.

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

After we execute `make` and run the server with `./server`, we can make requests with `curl`:

example 1: read the file `index.html` in root path `./html/`.
```
curl localhost:8080/index.html

<html>Hello from workflow and srpc file server!</html>
```

example 2: read the file `/a/b/`. This does't exist, so the error page `404.html` for `404` will be return as we fill them into `server.conf` above.

```
curl -i localhost:8080/a/b/
HTTP/1.1 404 Not Found
Server: SRPC HTTP File Server
Content-Length: 59
Connection: Keep-Alive

<html>This is not the web page you are looking for.</html>
```

The log printed by server:

```
./server 
http file service start, port 8080
file service get request: /a/b/
```

## 10. COMPUTE COMMAND

Next is a simple calculation server, similarly, the calculation will not block the server processing thread.

```
./srpc compute compute_test
```

Through the above command, we can create a project, which receives the url request as the parameter n, and performs Fibonacci calculation.

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

Enter the `compute_test/` directory and use `make` to compile and execute `./server` to run. Then we can use curl or browser to enter `localhost:8080/8` to calculate the 8th Fibonacci number. Here take curl as an example:

```
curl localhost:8080/8

<html><p>0 + 1 = 1.</p><p>1 + 1 = 2.</p><p>1 + 2 = 3.</p><p>2 + 3 = 5.</p><p>3 + 5 = 8.</p><p>5 + 8 = 13.</p><p>The No. 8 Fibonacci number is: 13.</p></html>
```

We can see the calculation steps inside the server. This computing example of the server uses go_task to encapsulate a function. Welcome to try more computing scheduling. 

