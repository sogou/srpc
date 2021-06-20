[English version](/docs/en/tutorial-05-context.md)

## RPC Context
- RPCContext专门用来辅助异步接口，Service和Client通用
- 每一个异步接口都会提供Context，用来给用户提供更高级的功能，比如获取对方ip、获取连接seqid等
- Context上一些功能是Server或Client独有的，比如Server可以设置回复数据的压缩方式，Client可以获取请求成功或失败
- Context上可以通过get_series获得所在的series，与workflow的异步模式无缝结合

### RPCContext API - Common
#### ``long long get_seqid() const;``
请求+回复视为1次完整通信，获得当前socket连接上的通信sequence id，seqid=0代表第1次

#### ``std::string get_remote_ip() const;``
获得对方IP地址，支持ipv4/ipv6

#### ``int get_peer_addr(struct sockaddr *addr, socklen_t *addrlen) const;``
获得对方地址，in/out参数为更底层的数据结构sockaddr

#### ``const std::string& get_service_name() const;``
获取RPC Service Name

#### ``const std::string& get_method_name() const;``
获取RPC Methode Name

#### ``SeriesWork *get_series() const;``
获取当前ServerTask/ClientTask所在series

### RPCContext API - Only for client done
#### ``bool success() const;``
client专用。这次请求是否成功

#### ``int get_status_code() const;``
client专用。这次请求的rpc status code

#### ``const char *get_errmsg() const;``
client专用。这次请求的错误信息

#### ``int get_error() const;``
client专用。这次请求的错误码

#### ``void *get_user_data() const;``
client专用。获取ClientTask的user_data。如果用户通过create_xxx_task接口产生task，则可以通过user_data域记录上下文，在创建task时设置，在回调函数中拿回。

### RPCContext API - Only for server process
#### ``void set_data_type(RPCDataType type);``
Server专用。设置数据打包类型
- RPCDataProtobuf
- RPCDataThrift
- RPCDataJson

#### ``void set_compress_type(RPCCompressType type);``
Server专用。设置数据压缩类型(注：Client的压缩类型在Client或Task上设置)
- RPCCompressNone
- RPCCompressSnappy
- RPCCompressGzip
- RPCCompressZlib
- RPCCompressLz4

#### ``void set_attachment_nocopy(const char *attachment, size_t len);``
Server专用。设置attachment附件。

#### ``bool get_attachment(const char **attachment, size_t *len) const;``
Server专用。获取attachment附件。

#### ``void set_reply_callback(std::function<void (RPCContext *ctx)> cb);``
Server专用。设置reply callback，操作系统写入socket缓冲区成功后被调用。

#### ``void set_send_timeout(int timeout);``
Server专用。设置发送超时，单位毫秒。-1代表无限。

#### ``void set_keep_alive(int timeout);``
Server专用。设置连接保活时间，单位毫秒。-1代表无限。

