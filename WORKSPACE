workspace(name = "srpc")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_proto",
    sha256 = "d8992e6eeec276d49f1d4e63cfa05bbed6d4a26cfe6ca63c972827a0d141ea3b",
    strip_prefix = "rules_proto-cfdc2fa31879c0aebe31ce7702b1a9c8a4be02d2",
    urls = [
        "https://github.com/bazelbuild/rules_proto/archive/cfdc2fa31879c0aebe31ce7702b1a9c8a4be02d2.tar.gz",
    ],
)
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
rules_proto_dependencies()
rules_proto_toolchains()

git_repository(
    name = "workflow",
    commit = "90cab0cb9a7567ac56a39ae2361d2dc1f4c3ee7b",
    remote = "https://github.com/sogou/workflow.git")

new_git_repository(
    name = "lz4",
    build_file = "@//third_party:lz4.BUILD",
    tag = "v1.9.3",
    remote = "https://github.com/lz4/lz4.git")

new_git_repository(
    name = "snappy",
    build_file = "@//third_party:snappy.BUILD",
    tag = "1.1.9",
    remote = "https://github.com/google/snappy.git")
