cmake_minimum_required(VERSION 3.6)
project(message)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PROTO_LIST rpc_meta.proto rpc_meta_brpc.proto rpc_span.proto rpc_meta_trpc.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_LIST})

set(SRC
	rpc_message_brpc.cc
	rpc_message_srpc.cc
	rpc_message_thrift.cc
	rpc_message_trpc.cc
	${PROTO_SRCS} ${PROTO_HDRS}
)

add_library(${PROJECT_NAME} OBJECT ${SRC})
if (WIN32)
	target_compile_definitions(
		${PROJECT_NAME} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

