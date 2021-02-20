cmake_minimum_required(VERSION 3.6)
project(module)

set(SRC
	rpc_span_policies.cc
)

add_library(${PROJECT_NAME} OBJECT ${SRC})

