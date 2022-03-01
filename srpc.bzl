"""
Rules for building C++ srpc with Bazel.
"""

load("@rules_cc//cc:defs.bzl", "cc_library")

tool_path = ":srpc_generator"

def srpc_cc_library(
        name,
        srcs,
        deps = [],
        type = "proto",
        out_prefix = "",
        visibility = None):
    output_directory = (
        ("$(@D)/%s" % (out_prefix)) if len(srcs) > 1 else ("$(@D)")
    )

    proto_output_headers = [
        (out_prefix + "%s.srpc.h") % (s.replace(".%s" % type, "").split("/")[-1])
        for s in srcs
    ]
    thrift_output_headers = [
        (out_prefix + "%s.thrift.h") % (s.replace(".%s" % type, "").split("/")[-1])
        for s in srcs
    ]

    if type == "thrift":
        output_headers = proto_output_headers + thrift_output_headers
        gen_proto = "thrift"
    if type == "proto":
        output_headers = proto_output_headers
        gen_proto = "protobuf"

    genrule_cmd = " ".join([
        "SRCS=($(SRCS));",
        "for f in $${SRCS[@]:0:%s}; do" % len(srcs),
        "$(location %s)" % (tool_path),
        " %s " % gen_proto,
        "$$f",
        output_directory + ";",
        "done",
    ])

    srcs_lib = "%s_srcs" % (name)

    native.genrule(
        name = srcs_lib,
        srcs = srcs,
        outs = output_headers,
        tools = [tool_path],
        cmd = genrule_cmd,
        output_to_bindir = True,
        message = "Generating srpc files for %s:" % (name),
    )

    runtime_deps = deps + [":libsrpc"]
    print(runtime_deps)

    cc_library(
        name = name,
        hdrs = [
            ":" + srcs_lib,
        ],
        srcs = [
            ":" + srcs_lib,
        ],
        features = [
            "-parse_headers",
        ],
        deps = runtime_deps,
        includes = [],
        linkstatic = 1,
        visibility = visibility,
    )
