load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(":srpc.bzl", "srpc_cc_library")

proto_library(
    name = "message_proto",
    srcs = [
        "src/message/rpc_meta.proto",
        "src/message/rpc_meta_brpc.proto",
        "src/message/rpc_meta_trpc.proto",
        "src/message/rpc_span.proto",
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
        "src/module/opentelemetry_common.proto",
        "src/module/opentelemetry_resource.proto",
        "src/module/opentelemetry_trace.proto",
        "src/module/opentelemetry_trace_service.proto",
    ],
    strip_import_prefix = "src/module",
)

cc_proto_library(
    name = "ModuleProto",
    deps = [":module_proto"],
)

cc_library(
    name = "srpc_hdrs",
    hdrs = glob(["src/include/srpc/*"]),
    includes = ["src/include"],
    visibility = ["//visibility:public"],
    deps = [
        "@workflow//:workflow_hdrs",
    ],
)

cc_library(
    name = "libsrpc",
    srcs = glob(["src/**/*.cc"]),
    hdrs = glob([
        "src/**/*.h",
        "src/**/*.inl",
    ]),
    includes = [
        "src",
        "src/compress",
        "src/message",
        "src/module",
        "src/thrift",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":MessageProto",
        ":ModuleProto",
        "@lz4",
        "@snappy",
        "@workflow//:http",
        "@workflow//:redis",
        "@workflow//:upstream",
    ],
)

cc_library(
    name = "srpc_generator_lib",
    srcs = glob(
        [
            "src/generator/*.cc",
        ],
        exclude = [
            "src/compiler.cc",
        ],
    ),
    hdrs = glob([
        "src/generator/*.h",
    ]),
    includes = ["src/generator"],
    visibility = ["//visibility:public"],
    deps = [
        ":libsrpc",
    ],
)

cc_binary(
    name = "srpc_generator",
    srcs = ["src/generator/compiler.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":libsrpc",
        ":srpc_generator_lib",
    ],
)

proto_library(
    name = "echo_pb_proto",
    srcs = [
        "tutorial/echo_pb.proto",
    ],
    strip_import_prefix = "tutorial",
)

cc_proto_library(
    name = "EchoProto",
    deps = [":echo_pb_proto"],
)

srpc_cc_library(
    name = "echo_pb_srpc",
    srcs = ["tutorial/echo_pb.proto"],
    deps = [":EchoProto"],
)

cc_binary(
    name = "srpc_pb_server",
    srcs = ["tutorial/tutorial-01-srpc_pb_server.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "srpc_pb_client",
    srcs = ["tutorial/tutorial-02-srpc_pb_client.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

srpc_cc_library(
    name = "echo_thrift_srpc",
    srcs = ["tutorial/echo_thrift.thrift"],
    type = "thrift",
)

cc_binary(
    name = "srpc_thrift_server",
    srcs = ["tutorial/tutorial-03-srpc_thrift_server.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_thrift_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "srpc_thrift_client",
    srcs = ["tutorial/tutorial-04-srpc_thrift_client.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_thrift_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "brpc_pb_server",
    srcs = ["tutorial/tutorial-05-brpc_pb_server.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "brpc_pb_client",
    srcs = ["tutorial/tutorial-06-brpc_pb_client.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "thrift_thrift_server",
    srcs = ["tutorial/tutorial-07-thrift_thrift_server.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_thrift_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "thrift_thrift_client",
    srcs = ["tutorial/tutorial-08-thrift_thrift_client.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_thrift_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "client_task",
    srcs = ["tutorial/tutorial-09-client_task.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "server_async",
    srcs = ["tutorial/tutorial-10-server_async.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":echo_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

proto_library(
    name = "helloworld_proto",
    srcs = [
        "tutorial/helloworld.proto",
    ],
    strip_import_prefix = "tutorial",
)

cc_proto_library(
    name = "HelloworldProto",
    deps = [":helloworld_proto"],
)

srpc_cc_library(
    name = "helloworld_pb_srpc",
    srcs = ["tutorial/helloworld.proto"],
    deps = [":HelloworldProto"],
)

cc_binary(
    name = "trpc_pb_server",
    srcs = ["tutorial/tutorial-11-trpc_pb_server.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":helloworld_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "trpc_pb_client",
    srcs = ["tutorial/tutorial-12-trpc_pb_client.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":helloworld_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "trpc_http_server",
    srcs = ["tutorial/tutorial-13-trpc_http_server.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":helloworld_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)

cc_binary(
    name = "trpc_http_client",
    srcs = ["tutorial/tutorial-14-trpc_http_client.cc"],
    linkopts = [
        "-lpthread",
        "-lssl",
        "-lcrypto",
    ],
    deps = [
        ":helloworld_pb_srpc",
        ":libsrpc",
        ":srpc_hdrs",
    ],
)
