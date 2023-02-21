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
    "redis" - create redis client
    "mysql" - create mysql client
    "kafka" - create kafka client
    "file"  - create project with file service
    "extra" - TODO
```

#### 3. START

Execute this simple example

```sh
./srpc-ctl http HTTP_EXAMPLE -o /root/
```

And we will get this on the screen, new project is in `/root/HTTP_EXAMPLE`.

```
Success: make project done. path = /root/HTTP_EXAMPLE
```

#### 4. COMMANDS

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

