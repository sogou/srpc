# srpc_tools

### A handy generator for Workflow and SRPC framework

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
    ./srpc-ctl <COMMAND> <PROJECT_NAME> [FLAGS]

Available Commands:
    "http"  - create project with both client and server
    "rpc"   - create project with both client and server
    "proxy" - create proxy for some client and server protocol
    "file"  - create project with file service
```

#### 3. START

Execute this simple example

```sh
./srpc-ctl http HTTP_EXAMPLE -o /root/
```

And we will get this on the screen, new project is in `/root/HTTP_EXAMPLE`.

```
Success:
      make project path " /root/HTTP_EXAMPLE/ " done.

Commands:
      cd /root/HTTP_EXAMPLE/
      make -j

Execute:
      ./server
      ./client
```

And we can try accorrding to the suggestions above.

#### 4. HTTP COMMANDS

commands for HTTP:

```
./srpc-ctl http
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc-ctl http <PROJECT_NAME> [FLAGS]

Available Flags:
    -o :    project output path (default: CURRENT_PATH)
    -d :    path of dependencies (default: COMPILE_PATH)
```

#### 5. RPC COMMANDS

commands for RPCs:

```
./srpc-ctl rpc
```

will get the following instructions:

```
Missing: PROJECT_NAME

Usage:
    ./srpc-ctl rpc <PROJECT_NAME> [FLAGS]

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
./srpc-ctl rpc rpc_example -f test_proto/test.proto 
```

And we can see the infomation when generating files, which is similar to srpc_genrator.

```
Info: srpc-ctl generator begin.
proto file: [/root/srpc/tools/rpc_example/test.proto]
Successfully parse service block [message] : EchoRequest
Successfully parse service block [message] : EchoResponse
Successfully parse service block [service] : new_test
Successfully parse method:Echo req:EchoRequest resp:EchoResponse
finish parsing proto file: [/root/srpc/tools/rpc_example/test.proto]
[Generator] generate srpc files: /root/srpc/tools/rpc_example/test.srpc.h 
[Generator Done]
[Generator] generate server files: /root/srpc/tools/rpc_example/server_main.cc, client files: /root/srpc/tools/rpc_example/client_main.cc
Info: srpc-ctl generator done.

Success:
      make project path " /root/srpc/tools/rpc_example/ " done.

Commands:
      cd /root/srpc/tools/rpc_example/
      make -j

Execute:
      ./server
      ./client
```

