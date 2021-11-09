cmake_minimum_required(VERSION 3.6)
project(module)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PROTO_LIST
	opentelemetry_common.proto
	opentelemetry_resource.proto
	opentelemetry_trace.proto
	opentelemetry_trace_service.proto)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_LIST})

set(SRC
	rpc_module.cc
	rpc_span_policies.cc
	${PROTO_SRCS} ${PROTO_HDRS}
)

add_library(${PROJECT_NAME} OBJECT ${SRC})

