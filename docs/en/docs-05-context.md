[中文版](/docs/tutorial-05-context.md)

## RPC Context

- RPCContext is used specially to assist asynchronous interfaces, and can be used in both Service and Client.
- Each asynchronous interface will provide a Context, which offers higher-level functions, such as obtaining the remote IP, the connection seqid, and so on.
- Some functions on Context are unique to Server or Client. For example, you can set the compression mode of the response data on Server, and you can obtain the success or failure status of a request on Client.
- On the Context, you can use ``get_series()`` to obtain the SeriesWork, which is seamlessly integrated with the asynchronous mode of Workflow.

### RPCContext API - Common

#### `long long get_seqid() const;`

One complete communication consists of request+response. The sequence id of the communication on the current socket connection can be obtained, and seqid=0 indicates the first communication.

#### `std::string get_remote_ip() const;`

Get the remote IP address. IPv4/IPv6 is supported.

#### `int get_peer_addr(struct sockaddr *addr, socklen_t *addrlen) const;`

Get the remote address. The in/out parameter is the lower-level data structure sockaddr.

#### `const std::string& get_service_name() const;`

Get RPC Service Name.

#### `const std::string& get_method_name() const;`

Get RPC Method Name.

#### `SeriesWork *get_series() const;`

Get the SeriesWork of the current ServerTask/ClientTask.

### RPCContext API - Only for client done

#### `bool success() const;`

For client only. The success or failure of the request.

#### `int get_status_code() const;`

For client only. The rpc status code of the request.

#### `const char *get_errmsg() const;`

For client only. The error info of the request.

#### `int get_error() const;`

For client only. The error code of the request.

#### `void *get_user_data() const;`

For client only. Get the user\_data of the ClientTask. If a user generates a task through the ``create_xxx_task()`` interface, the context can be recorded in the user_data field. You can set that field when creating the task, and retrieve it in the callback function.

### RPCContext API - Only for server process

#### `void set_data_type(RPCDataType type);`

For Server only. Set the data packaging type

- RPCDataProtobuf
- RPCDataThrift
- RPCDataJson

#### `void set_compress_type(RPCCompressType type);`

For Server only. Set the data compression type (note: the compression type for the Client is set on Client or Task)

- RPCCompressNone
- RPCCompressSnappy
- RPCCompressGzip
- RPCCompressZlib
- RPCCompressLz4

#### `void set_attachment_nocopy(const char *attachment, size_t len);`

For Server only. Set the attachment.

#### `bool get_attachment(const char **attachment, size_t *len) const;`

For Server only. Get the attachment.

#### `void set_reply_callback(std::function<void (RPCContext *ctx)> cb);`

For Server only. Set reply callback, which is called after the operating system successfully writes the data into the socket buffer.

#### `void set_send_timeout(int timeout);`

For Server only. Set the maximum time for sending the message, in milliseconds. -1 indicates unlimited time.

#### `void set_keep_alive(int timeout);`
For Server only. Set the maximum connection keep-alive time,  in milliseconds. -1 indicates unlimited time.

