def gen_srpc_pb_cc(name, files, deps_lib):
    native.genrule(
        name = name,
        srcs = files,
        outs = [
            name + ".srpc.h",
            "client." + name + ".srpc.cc",
            "server." + name + ".srpc.cc",
        ],
        cmd = "$(location srpc_generator) protobuf $(<) ./ && mv " + name + ".srpc.h $(location " + name + ".srpc.h) && mv client.pb_skeleton.cc $(location client." + name + ".srpc.cc) && mv server.pb_skeleton.cc $(location server." + name + ".srpc.cc)",
        tools = [":srpc_generator"],
    )
    
    native.cc_library(
        name = name + "_client_cc",
        srcs = [
            "client." + name + ".srpc.cc",
        ],
        hdrs = [
            name + ".srpc.h",
        ],
        deps = [
            ':srpc_hdrs',
        ] + deps_lib,
    )

    native.cc_library(
        name = name + "_server_cc",
        srcs = [
            "server." + name + ".srpc.cc",
        ],
        hdrs = [
            name + ".srpc.h",
        ],
        deps = [
            ':srpc_hdrs',
        ] + deps_lib,
    )

def gen_srpc_thrift_cc(name, files, deps_lib):
    native.genrule(
        name = name,
        srcs = files,
        outs = [
            name + ".srpc.h",
            name + ".thrift.h",
            "client." + name + ".thrift.cc",
            "server." + name + ".thrift.cc",
        ],
        cmd = "$(location srpc_generator) thrift $(<) ./ &&  mv " + name + ".thrift.h $(location " + name + ".thrift.h) && mv " + name + ".srpc.h $(location " + name + ".srpc.h) && mv client.thrift_skeleton.cc $(location client." + name + ".thrift.cc) && mv server.thrift_skeleton.cc $(location server." + name + ".thrift.cc)",
        tools = [":srpc_generator"],
    )
    
    native.cc_library(
        name = name + "_client_cc",
        srcs = [
            "client." + name + ".thrift.cc",
        ],
        hdrs = [
            name + ".srpc.h",
            name + ".thrift.h",
        ],
        deps = [
            ':srpc_hdrs',
        ] + deps_lib,
    )

    native.cc_library(
        name = name + "_server_cc",
        srcs = [
            "server." + name + ".thrift.cc",
        ],
        hdrs = [
            name + ".srpc.h",
            name + ".thrift.h",
        ],
        deps = [
            ':srpc_hdrs',
        ] + deps_lib,
    )
