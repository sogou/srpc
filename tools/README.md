# srpc-ctl

### An easy tool to generate Workflow and SRPC project

#### 1. COMPILE

```
git clone --recursive https://github.com/sogou/srpc.git
cd srpc/tools
make
```

#### 2. USAGE

```
./srpc-ctl
```

```
Description:
    A handy generator for Workflow and SRPC project.

Usage:
    ./srpc <COMMAND> <PROJECT_NAME> [FLAGS]

Available Commands:
    "http"  - create project with both client and server
    "rpc"   - create project with both client and server
    "proxy" - create proxy for some client and server protocol
    "file"  - create project with file service
```

#### 3. START

Execute this simple example

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
├── example.conf
├── server.conf
└── server_main.cc

2 directories, 12 files
```

And we can try to make the project accorrding to the suggestions above.

#### 4. HTTP COMMANDS

commands for HTTP:

```
./srpc http
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc http <PROJECT_NAME> [FLAGS]

Available Flags:
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

#### 5. RPC COMMANDS

commands for RPCs:

```
./srpc rpc
```

will get the following instructions:

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

#### 6. PROXY COMMANDS

commands for RPCs:

```
./srpc proxy
```

will get the following instructions:

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
