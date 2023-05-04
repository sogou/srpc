cmake_minimum_required(VERSION 3.6)
project(module)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PROTO_LIST
	proto/opentelemetry_common.proto
	proto/opentelemetry_resource.proto
	proto/opentelemetry_trace.proto
	proto/opentelemetry_metrics.proto
	proto/opentelemetry_metrics_service.proto)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_LIST})

set(SRC
	rpc_module.cc
	rpc_trace_module.cc
	rpc_metrics_module.cc
	rpc_trace_filter.cc
	rpc_metrics_filter.cc
	${PROTO_SRCS} ${PROTO_HDRS}
)

add_library(${PROJECT_NAME} OBJECT ${SRC})

