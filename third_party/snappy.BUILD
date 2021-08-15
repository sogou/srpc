genrule(
    name = "snappy_stubs_public_h",
    srcs = [
        "snappy-stubs-public.h.in",
    ],
    outs = [
        "snappy-stubs-public.h",
    ],
    cmd = "sed 's/$${HAVE_SYS_UIO_H_01}/true/g' $(<) | " +
          "sed 's/$${PROJECT_VERSION_MAJOR}/0/g' | " +
          "sed 's/$${PROJECT_VERSION_MINOR}/9/g' | " +
          "sed 's/$${PROJECT_VERSION_PATCH}/2/g' >$(@)",
)


cc_library(
    name = "snappy",
    srcs = [
        "snappy.cc",
        "snappy-c.cc",
        "snappy-sinksource.cc",
        "snappy-stubs-internal.cc",
    ],
    hdrs = [
        ":snappy_stubs_public_h",
        "snappy.h",
        "snappy-c.h",
        "snappy-internal.h",
        "snappy-sinksource.h",
        "snappy-stubs-internal.h",
        "snappy-stubs-public.h.in",
    ],
    copts = [
        "-Wno-non-virtual-dtor",
        "-Wno-unused-variable",
        "-Wno-implicit-fallthrough",
        "-Wno-unused-function",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)
