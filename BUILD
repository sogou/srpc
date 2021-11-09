load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(":srpc.bzl", "gen_srpc_pb_cc")
load(":srpc.bzl", "gen_srpc_thrift_cc")

proto_library(
    name = "message_proto",
    srcs = [
        'src/message/rpc_meta.proto',
        'src/message/rpc_meta_brpc.proto',
        'src/message/rpc_meta_trpc.proto',
        'src/message/rpc_span.proto',
    ],
    strip_import_prefix = "src/message",
)

cc_proto_library(
    name = "MessageProto",
    deps = [":message_proto"],
)

proto_library(
    name = "module_proto",
    srcs = [
        'src/module/opentelemetry_common.proto',
        'src/module/opentelemetry_resource.proto',
        'src/module/opentelemetry_trace.proto',
        'src/module/opentelemetrytrace_service.proto',
    ],
    strip_import_prefix = "src/module",
)

cc_proto_library(
    name = "ModuleProto",
    deps = [":module_proto"],
)


cc_library(
    name = 'srpc_hdrs',
    hdrs = glob(['src/include/srpc/*']),
    includes = ['src/include'],
    deps = [
        '@workflow//:workflow_hdrs',
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = 'libsrpc',
    srcs = glob(['src/**/*.cc']),
    hdrs = glob([
        'src/**/*.h',
        'src/**/*.inl',
    ]),
    includes = ['src', 'src/thrift', 'src/compress', 'src/message', 'src/module'],
    deps = [
        '@workflow//:http',
        '@workflow//:upstream',
        '@workflow//:redis',
        '@lz4//:lz4',
        '@snappy//:snappy',
        ':MessageProto',
        ':ModuleProto',
    ],
    visibility = ["//visibility:public"],
)

cc_binary(
    name ='srpc_generator',
    srcs = glob(['src/generator/*.cc']),
    deps = [':libsrpc'],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "echo_pb_proto",
    srcs = [
        'tutorial/echo_pb.proto',
    ],
    strip_import_prefix = "tutorial",
)

cc_proto_library(
    name = "EchoProto",
    deps = [":echo_pb_proto"],
)

gen_srpc_pb_cc(
    name = "echo_pb",
    files = ["tutorial/echo_pb.proto",],
    deps_lib = [':EchoProto'],
)

cc_binary(
    name = 'srpc_pb_server',
    srcs = ['tutorial/tutorial-01-srpc_pb_server.cc'],
    deps = [
        ':libsrpc',
        ':echo_pb_server_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'srpc_pb_client',
    srcs = ['tutorial/tutorial-02-srpc_pb_client.cc'],
    deps = [
        ':libsrpc',
        ':echo_pb_client_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

gen_srpc_thrift_cc(
    name = "echo_thrift",
    files = ["tutorial/echo_thrift.thrift",],
    deps_lib = [],
)

cc_binary(
    name = 'srpc_thrift_server',
    srcs = ['tutorial/tutorial-03-srpc_thrift_server.cc'],
    deps = [
        ':libsrpc',
        ':echo_thrift_server_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'srpc_thrift_client',
    srcs = ['tutorial/tutorial-04-srpc_thrift_client.cc'],
    deps = [
        ':libsrpc',
        ':echo_thrift_client_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'brpc_pb_server',
    srcs = ['tutorial/tutorial-05-brpc_pb_server.cc'],
    deps = [
        ':libsrpc',
        ':echo_pb_server_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'brpc_pb_client',
    srcs = ['tutorial/tutorial-06-brpc_pb_client.cc'],
    deps = [
        ':libsrpc',
        ':echo_pb_client_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'thrift_thrift_server',
    srcs = ['tutorial/tutorial-07-thrift_thrift_server.cc'],
    deps = [
        ':libsrpc',
        ':echo_thrift_server_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'thrift_thrift_client',
    srcs = ['tutorial/tutorial-08-thrift_thrift_client.cc'],
    deps = [
        ':libsrpc',
        ':echo_thrift_client_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'client_task',
    srcs = ['tutorial/tutorial-09-client_task.cc'],
    deps = [
        ':libsrpc',
        ':echo_pb_client_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'server_async',
    srcs = ['tutorial/tutorial-10-server_async.cc'],
    deps = [
        ':libsrpc',
        ':echo_pb_server_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

proto_library(
    name = "helloworld_proto",
    srcs = [
        'tutorial/helloworld.proto',
    ],
    strip_import_prefix = "tutorial",
)

cc_proto_library(
    name = "HelloworldProto",
    deps = [":helloworld_proto"],
)

gen_srpc_pb_cc(
    name = "helloworld",
    files = ["tutorial/helloworld.proto",],
    deps_lib = [':HelloworldProto'],
)

cc_binary(
    name = 'trpc_pb_server',
    srcs = ['tutorial/tutorial-11-trpc_pb_server.cc'],
    deps = [
        ':libsrpc',
        ':helloworld_server_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'trpc_pb_client',
    srcs = ['tutorial/tutorial-12-trpc_pb_client.cc'],
    deps = [
        ':libsrpc',
        ':helloworld_client_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'trpc_http_server',
    srcs = ['tutorial/tutorial-13-trpc_http_server.cc'],
    deps = [
        ':libsrpc',
        ':helloworld_server_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

cc_binary(
    name = 'trpc_http_client',
    srcs = ['tutorial/tutorial-14-trpc_http_client.cc'],
    deps = [
        ':libsrpc',
        ':helloworld_client_cc',
    ],
    linkopts = [
         '-lpthread',
         '-lssl',
         '-lcrypto',
    ],
)

